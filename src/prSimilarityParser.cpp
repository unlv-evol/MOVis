#include "headers/prSimilarityParser.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>

bool prSimilarityParser::load(const QString &filePath, QString *errorOut){
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorOut) {
            *errorOut = QStringLiteral("Failed to open similarity results file: %1")
            .arg(filePath);
        }
        return false;
    }

    reports.clear();
    prTitle.clear();
    prLocation.clear();

    const QByteArray contents = f.readAll();

    QJsonParseError parseError;
    const QJsonDocument json = QJsonDocument::fromJson(contents, &parseError);

    if (parseError.error == QJsonParseError::NoError && json.isObject()) {
        parseJson(json.object());
        return true;
    }

    if (filePath.endsWith(".json", Qt::CaseInsensitive)) {
        if (errorOut) {
            *errorOut = QStringLiteral("Invalid JSON similarity results file: %1 (%2)")
                .arg(filePath, parseError.errorString());
        }
        return false;
    }

    // Keep legacy pr_results.txt datasets readable. New runs always write
    // structured pr_results.json, but old result folders should continue
    // to open in the visualizer without a migration step.
    QString legacyText = QString::fromUtf8(contents);
    QTextStream in(&legacyText, QIODevice::ReadOnly);
    parseStream(in);

    return true;
}

bool prSimilarityParser::query(const QString &fileName,
                               const QString &hunkName,
                               double *similarityOut,
                               QString *classificationOut){
    const FileReport *report = findFileReport(fileName);
    if (!report)
        return false;

    const HunkInfo *hunkInfo = findHunk(*report, hunkName);
    if (!hunkInfo || hunkInfo->similarityPercent < 0.0)
        return false;

    if (similarityOut)
        *similarityOut = hunkInfo->similarityPercent;

    if (classificationOut)
        *classificationOut = report->overallClassification;

    return true;
}

QStringList prSimilarityParser::availableFiles(){
    QStringList keys;
    keys.reserve(reports.size());
    for (const FileReport &rep : reports) {
        keys << rep.analyzedPath;
    }
    return keys;
}

void prSimilarityParser::parseJson(const QJsonObject &root){
    prTitle = root.value("prTitle").toString();
    prLocation = root.value("prLocation").toString();

    const QJsonArray files = root.value("files").toArray();

    for (const QJsonValue &fileValue : files) {
        if (!fileValue.isObject()) {
            continue;
        }

        const QJsonObject fileObject = fileValue.toObject();

        FileReport report;
        report.analyzedPath = fileObject.value("fileName").toString().trimmed();
        report.overallClassification =
            fileObject.value("classification").toString().trimmed();

        const QJsonArray checks = fileObject.value("similarityChecks").toArray();

        for (const QJsonValue &checkValue : checks) {
            if (!checkValue.isObject()) {
                continue;
            }

            const QJsonObject check = checkValue.toObject();
            const QString checkName =
                check.value("checkName").toString().trimmed();

            if (checkName.isEmpty() ||
                !check.value("similarityPercent").isDouble()) {
                continue;
            }

            HunkInfo info;
            info.similarityPercent =
                check.value("similarityPercent").toDouble(-1.0);
            report.hunks.insert(checkName, info);
        }

        if (!report.analyzedPath.isEmpty()) {
            reports.push_back(report);
        }
    }
}

void prSimilarityParser::parseStream(QTextStream &in){
    // Regexes based on your example.
    QRegularExpression reSimilarityAnalysis(
        R"(^\s*Similarity analysis for:\s+(.*)\s*$)");
    QRegularExpression reOverallClass(
        R"(^\s*-\s*Overall Classification is:\s*([A-Za-z0-9_]+)\s*$)");
    QRegularExpression reHunkLine(
        R"(^\s*-\s*(.+?)\s*\(\d+\)\s*-\s*has a similarity of:\s*([0-9]+(?:\.[0-9]+)?)%\s*$)");
    QRegularExpression rePRTitle(
        R"(^\s*PR\s*Title\s*:\s*(.+?)\s*$)");
    QRegularExpression rePRLocation(
        R"(^\s*PR\s*Location\s*:\s*(\S+)\s*$)");

    FileReport current;
    bool inFileBlock = false;

    while (!in.atEnd()) {
        const QString line = in.readLine();

        // PR metadata (usually appears once in the file)
        if (prTitle.isEmpty()) {
            QRegularExpressionMatch mt = rePRTitle.match(line);
            if (mt.hasMatch()) {
                prTitle = mt.captured(1).trimmed();
                continue;
            }
        }

        if (prLocation.isEmpty()) {
            QRegularExpressionMatch mu = rePRLocation.match(line);
            if (mu.hasMatch()) {
                prLocation = mu.captured(1).trimmed();
                continue;
            }
        }

        // Start of a new file block
        QRegularExpressionMatch mSim = reSimilarityAnalysis.match(line);
        if (mSim.hasMatch()) {
            // If we were already in a block, store previous
            if (inFileBlock) {
                reports.push_back(current);
                current = FileReport();
            }
            inFileBlock = true;
            current.analyzedPath = mSim.captured(1).trimmed();
            continue;
        }

        if (!inFileBlock)
            continue;

        // Overall classification line
        QRegularExpressionMatch mClass = reOverallClass.match(line);
        if (mClass.hasMatch()) {
            current.overallClassification = mClass.captured(1).trimmed();
            continue;
        }

        // Hunk similarity lines
        QRegularExpressionMatch mHunk = reHunkLine.match(line);
        if (mHunk.hasMatch()) {
            QString hunkFile = mHunk.captured(1).trimmed();  // e.g., src/hunk_1_additions.scala
            double sim = mHunk.captured(2).toDouble();

            HunkInfo info;
            info.similarityPercent = sim;
            current.hunks.insert(hunkFile, info);
            continue;
        }
    }

    // Push last block if any
    if (inFileBlock) {
        reports.push_back(current);
    }
}

const FileReport* prSimilarityParser::findFileReport(const QString &fileName){
    const QString needle = fileName.trimmed();

    for (const FileReport &rep : reports) {
        // match full path contains needle or basename equals needle
        QString base = QFileInfo(rep.analyzedPath).fileName();
        if (rep.analyzedPath.contains(needle, Qt::CaseInsensitive) ||
            base.compare(needle, Qt::CaseInsensitive) == 0)
        {
            return &rep;
        }
    }
    return nullptr;
}

const HunkInfo* prSimilarityParser::findHunk(const FileReport &rep,
                                             const QString &hunkName){
    const QString needle = hunkName.trimmed();

    // Exact key match first
    QHash<QString, HunkInfo>::const_iterator itExact = rep.hunks.constFind(needle);
    if (itExact != rep.hunks.constEnd())
        return &itExact.value();

    // Otherwise substring match on keys
    for (QHash<QString, HunkInfo>::const_iterator it = rep.hunks.constBegin();
         it != rep.hunks.constEnd(); ++it) {
        if (it.key().contains(needle, Qt::CaseInsensitive)) {
            return &it.value();
        }
    }
    return nullptr;
}
