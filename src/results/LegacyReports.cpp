#include "headers/results/LegacyReports.h"

#include "headers/GACPD/Methods/appPath.h"
#include "headers/helpers/fileHelpers.h"
#include "headers/results/HunkPaths.h"
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSaveFile>
#include <QRegularExpression>
#include <QSet>
#include <QTemporaryDir>

static QSet<int> actualChangedLinesForVisualizerCheck(const QString &repoBaseAbs,
                                                const QString &checkName){
    const QString baseName = QFileInfo(normalizePath(checkName)).fileName();
    static const QRegularExpression pattern(
        R"(^(hunk_\d+)_(additions|deletions)(\.[^/\\]+)?$)",
        QRegularExpression::CaseInsensitiveOption
        );

    const QRegularExpressionMatch match = pattern.match(baseName);
    if (!match.hasMatch()) {
        return {};
    }

    const QString prefix = match.captured(1);
    const QString kind = match.captured(2).toLower();
    const QString extension = match.captured(3);
    const QString companionName =
        prefix + (kind == "additions" ? "_full_add" : "_full_del") + extension;

    QFile companion(QDir(QDir(repoBaseAbs).filePath("src")).filePath(companionName));
    if (!companion.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }

    QTextStream in(&companion);
    QSet<int> changedLines;
    int generatedLine = 0;

    while (!in.atEnd()) {
        const QString line = in.readLine();
        if (line.startsWith("@@")) {
            continue;
        }

        ++generatedLine;

        const bool actualChange =
            (kind == "additions" && line.startsWith("+") &&
             !line.startsWith("+++")) ||
            (kind == "deletions" && line.startsWith("-") &&
             !line.startsWith("---"));

        if (actualChange) {
            changedLines.insert(generatedLine);
        }
    }

    return changedLines;
}

static int visualizerJscpdRangeLine(const QJsonObject &fileObject,
                                    const QString &simpleKey,
                                    const QString &locationKey){
    const int simple = fileObject.value(simpleKey).toInt();
    if (simple > 0) {
        return simple;
    }

    return fileObject.value(locationKey).toObject().value("line").toInt();
}

static bool visualizerHasPinnedJscpd(const QString &root, const QString &executable){
    const QString binary = QDir(root).filePath(
        "node_modules/.bin/" + executable);
    if (!QFileInfo::exists(binary)) {
        return false;
    }

    QFile packageFile(
        QDir(root).filePath("node_modules/jscpd/package.json"));
    if (!packageFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    QJsonParseError error;
    const QJsonDocument document =
        QJsonDocument::fromJson(packageFile.readAll(), &error);
    return error.error == QJsonParseError::NoError &&
           document.isObject() &&
           document.object().value("version").toString() == "5.0.16";
}

static QString visualizerJscpdBin(){
#ifdef Q_OS_WIN
    const QString executable = QStringLiteral("jscpd.cmd");
#else
    const QString executable = QStringLiteral("jscpd");
#endif

    if (visualizerHasPinnedJscpd(movisLocalRoot(), executable)) {
        return QDir(movisLocalRoot()).filePath(
            "node_modules/.bin/" + executable);
    }

#ifdef Q_OS_MACOS
    if (visualizerHasPinnedJscpd(movisDependencyRoot(), executable)) {
        return QDir(movisDependencyRoot()).filePath(
            "node_modules/.bin/" + executable);
    }
#endif

    return QString();
}

static QString visualizerBundledNodeBinDir(){
#ifdef Q_OS_WIN
    return QDir(movisLocalRoot()).filePath("tools/node/windows/x64");
#elif defined(Q_OS_MACOS)
    return QDir(movisLocalRoot()).filePath("tools/node/macos/arm64/bin");
#elif defined(Q_OS_LINUX)
    return QDir(movisLocalRoot()).filePath("tools/node/linux/x64/bin");
#else
    return QString();
#endif
}

QHash<QString, bool> classificationClonePresenceByAddDeleteHunk(
    const QJsonArray &duplicates,
    const QString &repoBaseAbs,
    const JscpdMatchKeys &keys,
    bool localWorkspaceReport){
    QHash<QString, bool> clonePresenceByHunk;

    // Mirror GACPD's strict normal-classification rule: a JSCPD duplicate is
    // direct MO/ED evidence only if the duplicate range actually touches a
    // real '+' or '-' line. Unchanged context embedded in additions/deletions
    // files must not make the normal classifier look successful.
    const QDir srcDir(QDir(repoBaseAbs).filePath("src"));
    const QStringList checkNames = srcDir.entryList(
        QDir::Files | QDir::NoDotAndDotDot,
        QDir::Name
        );

    QHash<QString, QString> canonicalGeneratedByFoldedBase;
    QHash<QString, QSet<int>> changedLinesByCheck;

    for (const QString &checkName : checkNames) {
        canonicalGeneratedByFoldedBase.insert(
            checkName.toCaseFolded(), checkName);

        const QString kind = reportedHunkKind(checkName);
        if (kind != "additions" && kind != "deletions") {
            continue;
        }

        const QString displayKey = displayedHunkKey(checkName);
        if (!displayKey.isEmpty()) {
            clonePresenceByHunk.insert(displayKey, false);
            changedLinesByCheck.insert(
                checkName,
                actualChangedLinesForVisualizerCheck(repoBaseAbs, checkName));
        }
    }

    for (const QJsonValue &value : duplicates) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject duplicate = value.toObject();
        const QJsonObject firstFile = duplicate.value("firstFile").toObject();
        const QJsonObject secondFile = duplicate.value("secondFile").toObject();

        const QString firstReported =
            normalizePath(firstFile.value("name").toString());
        const QString secondReported =
            normalizePath(secondFile.value("name").toString());

        const QString firstBase = QFileInfo(firstReported).fileName();
        const QString secondBase = QFileInfo(secondReported).fileName();

        const QString firstGenerated = canonicalGeneratedByFoldedBase.value(
            firstBase.toCaseFolded());
        const QString secondGenerated = canonicalGeneratedByFoldedBase.value(
            secondBase.toCaseFolded());

        const bool firstIsGenerated = !firstGenerated.isEmpty();
        const bool secondIsGenerated = !secondGenerated.isEmpty();

        // Only generated hunk <-> divergent source contributes. This also
        // rejects additions/deletions <-> context/full_* local matches.
        if (firstIsGenerated == secondIsGenerated) {
            continue;
        }

        if (!localWorkspaceReport) {
            const QString &reportedCmp = firstIsGenerated
                ? secondReported
                : firstReported;
            if (!reportedPathMatchesRight(reportedCmp, keys)) {
                continue;
            }
        }

        const QString checkName =
            firstIsGenerated ? firstGenerated : secondGenerated;
        const QString kind = reportedHunkKind(checkName);
        if (kind != "additions" && kind != "deletions") {
            continue;
        }

        const QString displayKey = displayedHunkKey(checkName);
        if (displayKey.isEmpty()) {
            continue;
        }

        const QJsonObject checkFile =
            firstIsGenerated ? firstFile : secondFile;
        const int startLine = visualizerJscpdRangeLine(
            checkFile, "start", "startLoc");
        const int endLine = visualizerJscpdRangeLine(
            checkFile, "end", "endLoc");

        if (startLine <= 0 || endLine < startLine) {
            continue;
        }

        const QSet<int> &changedLines = changedLinesByCheck.value(checkName);
        bool touchesActualChange = false;
        for (int line = startLine; line <= endLine; ++line) {
            if (changedLines.contains(line)) {
                touchesActualChange = true;
                break;
            }
        }

        if (touchesActualChange) {
            clonePresenceByHunk[displayKey] = true;
        }
    }

    return clonePresenceByHunk;
}

bool rebuildContextDuplicatesForLegacyResult(const QString &repoBaseAbs,
                                             const QString &rightPath,
                                             QJsonArray &duplicatesOut){
    // Older datasets predate jscpd-context-report.json. Recreate the 10-token
    // scan in an isolated workspace containing ONLY hunk_N_context files and
    // the selected cmp file. Normal additions/deletions must never participate
    // in the context detector, even during legacy visualizer backfill.
    const QString jscpd = visualizerJscpdBin();
    if (!QFileInfo::exists(jscpd)) {
        qWarning() << "Cannot rebuild context report; JSCPD is missing:" << jscpd;
        return false;
    }

    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        qWarning() << "Cannot create temporary directory for context report";
        return false;
    }

    const QString scanSrc = QDir(tempDir.path()).filePath("src");
    const QString scanCmp = QDir(tempDir.path()).filePath("cmp");
    const QString scanReports = QDir(tempDir.path()).filePath("reports");
    QDir().mkpath(scanSrc);
    QDir().mkpath(scanCmp);
    QDir().mkpath(scanReports);

    static const QRegularExpression contextPattern(
        R"(^hunk_\d+_context(\.[^/\\]+)?$)",
        QRegularExpression::CaseInsensitiveOption
        );

    const QDir sourceDir(QDir(repoBaseAbs).filePath("src"));
    int copiedContexts = 0;
    for (const QString &fileName : sourceDir.entryList(
             QDir::Files | QDir::NoDotAndDotDot, QDir::Name)) {
        if (!contextPattern.match(fileName).hasMatch()) {
            continue;
        }

        if (QFile::copy(sourceDir.filePath(fileName),
                        QDir(scanSrc).filePath(fileName))) {
            ++copiedContexts;
        }
    }

    if (copiedContexts == 0 || rightPath.isEmpty() ||
        !QFileInfo::exists(rightPath)) {
        return false;
    }

    if (!QFile::copy(rightPath,
                     QDir(scanCmp).filePath(QFileInfo(rightPath).fileName()))) {
        return false;
    }

    QStringList args;
    const QString suffix = QFileInfo(rightPath).suffix();
    if (!suffix.isEmpty()) {
        args << "--pattern" << QString("*.%1").arg(suffix);
    }

    args << "--min-lines" << "1"
         << "--min-tokens" << "10"
         << "--reporters" << "json"
         << "--output" << scanReports
         << "--skip-local"
         << "src"
         << "cmp";

    QProcess process;
    process.setWorkingDirectory(tempDir.path());

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    const QString nodeBinDir = visualizerBundledNodeBinDir();
    if (!nodeBinDir.isEmpty()) {
        const QString oldPath = env.value("PATH");
#ifdef Q_OS_WIN
        env.insert("PATH", nodeBinDir + ";" + oldPath);
#else
        env.insert("PATH", nodeBinDir + ":" + oldPath);
#endif
    }
    process.setProcessEnvironment(env);
    process.start(jscpd, args);

    if (!process.waitForStarted()) {
        qWarning() << "Could not start JSCPD context backfill:"
                   << process.errorString();
        return false;
    }

    if (!process.waitForFinished(-1)) {
        qWarning() << "JSCPD context backfill did not finish:"
                   << process.errorString();
        return false;
    }

    const QStringList candidates = {
        QDir(scanReports).filePath("jscpd-report.json"),
        QDir(scanReports).filePath("html/jscpd-report.json")
    };

    QString generatedReportPath;
    for (const QString &candidate : candidates) {
        if (QFileInfo::exists(candidate)) {
            generatedReportPath = candidate;
            break;
        }
    }

    if (generatedReportPath.isEmpty()) {
        qWarning() << "JSCPD context backfill produced no JSON report. stderr:"
                   << process.readAllStandardError();
        return false;
    }

    QFile generated(generatedReportPath);
    if (!generated.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    const QByteArray reportBytes = generated.readAll();
    generated.close();

    QJsonParseError parseError;
    const QJsonDocument reportDoc =
        QJsonDocument::fromJson(reportBytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !reportDoc.isObject()) {
        qWarning() << "Invalid JSCPD context backfill JSON:"
                   << parseError.errorString();
        return false;
    }

    duplicatesOut = reportDoc.object().value("duplicates").toArray();

    // Cache the isolated 10-token report beside the existing report so old
    // data behaves like newly generated data on subsequent visualizer opens.
    const QString reportsDir = QDir(repoBaseAbs).filePath("reports");
    QDir().mkpath(reportsDir);
    const QString preservedPath =
        QDir(reportsDir).filePath("jscpd-context-report.json");

    QSaveFile preserved(preservedPath);
    if (preserved.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (preserved.write(reportBytes) == reportBytes.size()) {
            if (!preserved.commit()) {
                qWarning() << "Could not commit rebuilt context report:"
                           << preservedPath;
            }
        } else {
            preserved.cancelWriting();
        }
    }

    return true;
}
