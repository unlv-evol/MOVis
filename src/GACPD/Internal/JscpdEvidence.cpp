#include "headers/GACPD/Internal/JscpdEvidence.h"

#include "headers/helpers/fileHelpers.h"
#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QTextStream>

static bool generatedAddDeleteCheckParts(const QString &checkName,
                                         QString &prefix,
                                         QString &kind,
                                         QString &extension){
    const QString baseName = QFileInfo(checkName).fileName();
    static const QRegularExpression pattern(
        R"(^(hunk_\d+)_(additions|deletions)(\.[^/\\]+)?$)",
        QRegularExpression::CaseInsensitiveOption
        );

    const QRegularExpressionMatch match = pattern.match(baseName);
    if (!match.hasMatch()) {
        prefix.clear();
        kind.clear();
        extension.clear();
        return false;
    }

    prefix = match.captured(1);
    kind = match.captured(2).toLower();
    extension = match.captured(3);
    return true;
}

static QSet<int> actualChangedLinesForCheck(const ClassificationWorkspace &workspace,
                                            const QString &checkName){
    QString prefix;
    QString kind;
    QString extension;

    if (!generatedAddDeleteCheckParts(checkName, prefix, kind, extension)) {
        return {};
    }

    const QString companionName =
        prefix + (kind == "additions" ? "_full_add" : "_full_del") + extension;
    QFile companion(QDir(workspace.src).filePath(companionName));

    if (!companion.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }

    QSet<int> changedLines;
    QTextStream in(&companion);
    int generatedLine = 0;

    while (!in.atEnd()) {
        const QString line = in.readLine();

        // full_add/full_del has the @@ header; hunk_N_additions/deletions does
        // not. Every subsequent line maps one-to-one onto the generated check.
        if (line.startsWith("@@")) {
            continue;
        }

        ++generatedLine;

        const bool isActualChange =
            (kind == "additions" && line.startsWith("+") &&
             !line.startsWith("+++")) ||
            (kind == "deletions" && line.startsWith("-") &&
             !line.startsWith("---"));

        if (isActualChange) {
            changedLines.insert(generatedLine);
        }
    }

    return changedLines;
}

static int jscpdRangeLine(const QJsonObject &fileObject,
                          const QString &simpleKey,
                          const QString &locationKey){
    const int simple = fileObject.value(simpleKey).toInt();
    if (simple > 0) {
        return simple;
    }

    return fileObject.value(locationKey).toObject().value("line").toInt();
}

QString jscpdReportedBaseName(QString reportedName){
    reportedName.replace('\\', '/');
    return QFileInfo(QDir::cleanPath(reportedName)).fileName();
}

QHash<QString, bool> directClonePresenceByCheck(
    const QJsonObject &data,
    const ClassificationWorkspace &workspace){
    const QDir sourceDir(workspace.src);
    const QStringList generatedNames = sourceDir.entryList(
        QDir::Files | QDir::NoDotAndDotDot,
        QDir::Name
        );

    // Keep every generated filename in this map so hunk-to-hunk matches are
    // never mistaken for a generated-hunk-to-cmp match.
    QHash<QString, QString> generatedByFoldedBase;
    QHash<QString, QSet<int>> actualChangeLines;
    QHash<QString, bool> directPresence;

    for (const QString &name : generatedNames) {
        generatedByFoldedBase.insert(name.toCaseFolded(), name);

        QString prefix;
        QString kind;
        QString extension;
        if (generatedAddDeleteCheckParts(name, prefix, kind, extension)) {
            actualChangeLines.insert(name,
                                     actualChangedLinesForCheck(workspace, name));
            directPresence.insert(name, false);
        }
    }

    const QJsonArray duplicates = data.value("duplicates").toArray();

    for (const QJsonValue &value : duplicates) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject duplicate = value.toObject();
        const QJsonObject firstFile = duplicate.value("firstFile").toObject();
        const QJsonObject secondFile = duplicate.value("secondFile").toObject();

        const QString firstBase =
            jscpdReportedBaseName(firstFile.value("name").toString());
        const QString secondBase =
            jscpdReportedBaseName(secondFile.value("name").toString());

        const QString firstGenerated = generatedByFoldedBase.value(
            firstBase.toCaseFolded());
        const QString secondGenerated = generatedByFoldedBase.value(
            secondBase.toCaseFolded());

        const bool firstIsGenerated = !firstGenerated.isEmpty();
        const bool secondIsGenerated = !secondGenerated.isEmpty();

        // Only generated-hunk <-> divergent cmp matches are classification
        // evidence. Ignore generated-hunk <-> generated-hunk matches.
        if (firstIsGenerated == secondIsGenerated) {
            continue;
        }

        const QString checkName =
            firstIsGenerated ? firstGenerated : secondGenerated;
        if (!directPresence.contains(checkName)) {
            continue;
        }

        const QJsonObject checkFile =
            firstIsGenerated ? firstFile : secondFile;
        const int startLine = jscpdRangeLine(checkFile, "start", "startLoc");
        const int endLine = jscpdRangeLine(checkFile, "end", "endLoc");

        if (startLine <= 0 || endLine < startLine) {
            continue;
        }

        const QSet<int> &changeLines = actualChangeLines.value(checkName);
        for (int line = startLine; line <= endLine; ++line) {
            if (changeLines.contains(line)) {
                directPresence[checkName] = true;
                break;
            }
        }
    }

    return directPresence;
}
