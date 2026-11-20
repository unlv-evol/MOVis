#include "headers/results/ResultFiles.h"

#include "headers/helpers/fileHelpers.h"
#include "headers/helpers/prHelpers.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

QString individualResultField(const QString &resultsPath, const QString &fieldName){
    QFile file(resultsPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject())
        return QString();

    return doc.object().value(fieldName).toString().trimmed();
}

QString individualResultErrorSummary(const QString &resultsPath){
    QFile file(resultsPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return QStringLiteral("Could not open the saved error result.");

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject())
        return QStringLiteral("The saved error result is not valid JSON.");

    const QJsonObject result = doc.object();
    const QString explicitReason =
        result.value("errorReason").toString().trimmed();
    const QString patchStatus =
        result.value("patchStatus").toString().trimmed();

    QString summary = explicitReason;

    if (summary.isEmpty()) {
        if (patchStatus == "missing_from_github") {
            summary = QStringLiteral(
                "GitHub reported textual changes but did not provide patch text."
                );
        } else if (patchStatus == "available_but_archive_write_failed") {
            summary = QStringLiteral(
                "The patch was fetched, but MOVis could not preserve it in the result workspace."
                );
        } else if (patchStatus == "missing_file_entry") {
            summary = QStringLiteral(
                "No GitHub file metadata entry was available for this file."
                );
        } else if (!patchStatus.isEmpty()) {
            summary = QString("Patch status: %1").arg(patchStatus);
        } else {
            summary = QStringLiteral(
                "No explicit error reason was saved by this result."
                );
        }
    }

    if (!patchStatus.isEmpty() &&
        !summary.contains("Patch status:", Qt::CaseInsensitive)) {
        summary += QString("  [Patch status: %1]").arg(patchStatus);
    }

    return summary;
}

QSet<QString> directCloneHunksFromIndividualResult(
    const QString &resultsPath,
    bool &hasDirectCloneMetadata){
    QSet<QString> directHunks;
    hasDirectCloneMetadata = false;

    QFile file(resultsPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return directHunks;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        return directHunks;
    }

    static const QRegularExpression hunkPattern(
        R"(^(hunk_\d+)_(additions|deletions)(?:\.[^/\\]+)?$)",
        QRegularExpression::CaseInsensitiveOption
        );

    const QJsonArray checks =
        doc.object().value("similarityChecks").toArray();

    for (const QJsonValue &value : checks) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject check = value.toObject();
        if (!check.contains("directChangeClone")) {
            continue;
        }

        hasDirectCloneMetadata = true;
        if (!check.value("directChangeClone").toBool()) {
            continue;
        }

        const QString checkName =
            QFileInfo(check.value("checkName").toString()).fileName();
        const QRegularExpressionMatch match = hunkPattern.match(checkName);
        if (match.hasMatch()) {
            directHunks.insert(match.captured(1).toCaseFolded());
        }
    }

    return directHunks;
}

QString existingResultsFilePath(const QString &filePath){
    if (filePath.isEmpty() || !QFileInfo::exists(filePath))
        return QString();

    return QFileInfo(filePath).absoluteFilePath();
}

QString prResultsFileForSelection(const QString &rootDirectory,
                                  const QString &pr,
                                  const QString &classification){
    if (pr.isEmpty() || classification.isEmpty())
        return QString();

    const QString prRoot =
        buildDirWithPRAndClassification(rootDirectory, pr, classification);
    const QString jsonPath = QDir(prRoot).filePath("pr_results.json");
    if (QFileInfo::exists(jsonPath))
        return jsonPath;

    const QString legacyPath = QDir(prRoot).filePath("pr_results.txt");
    if (QFileInfo::exists(legacyPath))
        return legacyPath;

    return QString();
}

QString individualResultsForSelectedPath(const QString &rootDirectory,
                                         const QString &pr,
                                         const QString &prClassification,
                                         const QString &selectedPath){
    if (selectedPath.isEmpty())
        return QString();

    if (QFileInfo(selectedPath).fileName().compare(
            "results.json", Qt::CaseInsensitive) == 0) {
        return QFileInfo(selectedPath).absoluteFilePath();
    }

    const QString prRoot = buildDirWithPRAndClassification(
        rootDirectory, pr, prClassification);
    QDir prDir(prRoot);

    QString relative = prDir.relativeFilePath(selectedPath);
    relative = QDir::fromNativeSeparators(relative);

    if (relative == ".." || relative.startsWith("../"))
        return QString();

    const QStringList parts = relative.split('/', Qt::SkipEmptyParts);
    if (parts.size() < 2)
        return QString();

    const QString resultsPath = prDir.filePath(
        parts.at(0) + "/" + parts.at(1) + "/results.json");

    return QFileInfo::exists(resultsPath) ? resultsPath : QString();
}

bool isTerminalVisualizerClassification(const QString &classification){
    return classification == "CC" ||
           classification == "NE" ||
           classification == "ERROR";
}
