#include "headers/GACPD/Internal/ResultRecords.h"

#include "headers/GACPD/Internal/FileLanguages.h"
#include "headers/GACPD/Internal/OrderedJson.h"
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>

static QJsonArray similarityLinesToJson(const QStringList &similarityLines);
static QJsonArray hunkClassificationsToJson(
    const QVector<HunkClassificationInfo> &hunkClassifications);

static QString normalizePersistedPath(QString path){
    path = QDir::cleanPath(QDir::fromNativeSeparators(path));
    return path;
}

static QString persistedDivergentPath(
    const QString &divergentPath,
    const QString &divergentRepo){
    if (divergentPath.trimmed().isEmpty()) {
        return QString();
    }

    const QString normalizedPath = normalizePersistedPath(divergentPath);
    QString normalizedRepo = normalizePersistedPath(divergentRepo).trimmed();
    while (normalizedRepo.startsWith('/')) {
        normalizedRepo.remove(0, 1);
    }
    while (normalizedRepo.endsWith('/')) {
        normalizedRepo.chop(1);
    }

    if (normalizedRepo.isEmpty()) {
        return normalizedPath;
    }

    // Results/Repos_files is temporary working storage and is deleted after
    // each run. Persist only the divergent repository and the path inside it
    // so results.json never points at storage that intentionally disappears.
    if (normalizedPath == normalizedRepo ||
        normalizedPath.startsWith(normalizedRepo + "/")) {
        return normalizedPath;
    }

    const QString marker = "/" + normalizedRepo + "/";
    const int markerPos = normalizedPath.lastIndexOf(marker);
    if (markerPos >= 0) {
        return normalizedPath.mid(markerPos + 1);
    }

    const QString trailingMarker = "/" + normalizedRepo;
    if (normalizedPath.endsWith(trailingMarker)) {
        return normalizedRepo;
    }

    // Keep the original normalized value if a legacy/unexpected path cannot
    // be related to the configured divergent repository.
    return normalizedPath;
}

static QString defaultErrorReasonForPatchStatus(const QString &patchStatus){
    const QString status = patchStatus.trimmed();

    if (status == "missing_from_github") {
        return QStringLiteral(
            "GitHub reported textual changes, but did not provide patch text for this file."
            );
    }

    if (status == "available_but_archive_write_failed") {
        return QStringLiteral(
            "The patch was fetched, but MOVis could not preserve it in the result workspace."
            );
    }

    if (status == "missing_file_entry") {
        return QStringLiteral(
            "No GitHub file metadata entry was available for this file."
            );
    }

    if (!status.isEmpty()) {
        return QString("File classification failed (patch status: %1).").arg(status);
    }

    return QStringLiteral("File classification failed for an unspecified reason.");
}

static QJsonArray similarityLinesToJson(const QStringList &similarityLines){
    // Keep similarity checks machine-readable. The optional final group makes
    // this backward-compatible with the original three-field message while
    // also supporting the direct-change validation added for strict context
    // handling.
    static const QRegularExpression similarityPattern(
        R"(^\s*(.+?)\s*\((\d+)\)\s*-\s*has a similarity of:\s*([0-9]+(?:\.[0-9]+)?)%\s*(?:-\s*direct change clone:\s*(yes|no))?\s*$)",
        QRegularExpression::CaseInsensitiveOption
        );

    QJsonArray checks;

    for (const QString &rawLine : similarityLines) {
        const QString line = rawLine.trimmed();
        const QRegularExpressionMatch match = similarityPattern.match(line);

        QJsonObject check;
        if (match.hasMatch()) {
            check["checkName"] = match.captured(1).trimmed();
            check["tokenSize"] = match.captured(2).toInt();
            check["similarityPercent"] = match.captured(3).toDouble();

            const QString directChange = match.captured(4).trimmed();
            if (!directChange.isEmpty()) {
                check["directChangeClone"] =
                    directChange.compare("yes", Qt::CaseInsensitive) == 0;
            }
        } else if (!line.isEmpty()) {
            // Preserve unexpected legacy/custom lines rather than dropping
            // information if the similarity message format changes later.
            check["raw"] = line;
        }

        if (!check.isEmpty()) {
            checks.append(check);
        }
    }

    return checks;
}

static QJsonArray hunkClassificationsToJson(
    const QVector<HunkClassificationInfo> &hunkClassifications){
    QJsonArray hunks;

    for (const HunkClassificationInfo &hunk : hunkClassifications) {
        QJsonObject object;
        object["number"] = hunk.hunkNumber;
        object["classification"] = hunk.classification;
        object["additionsClassification"] = hunk.additionsClassification;
        object["deletionsClassification"] = hunk.deletionsClassification;
        hunks.append(object);
    }

    return hunks;
}

bool writeIndividualFileResultJson(
    const QString &path,
    const QString &fileName,
    const QString &prNr,
    const QString &mainline,
    const QString &divergentRepo,
    const QString &changeStatus,
    bool renamed,
    const QString &renamedFileName,
    const QString &divergentPath,
    const QString &divergentPathType,
    const QString &classification,
    bool patchAvailable,
    const QString &patchStatus,
    const QStringList &similarityLines,
    const QVector<HunkClassificationInfo> &hunkClassifications,
    const QString &errorReason){
    QJsonObject result;
    result["fileName"] = fileName;
    addFileLanguageMetadata(result, fileName);
    result["pr"] = prNr;
    result["mainline"] = mainline;
    result["divergentRepo"] = divergentRepo;
    result["changeStatus"] = changeStatus;
    result["renamed"] = renamed;
    result["renamedFileName"] = renamed ? renamedFileName : QString();
    result["patchAvailable"] = patchAvailable;
    result["patchStatus"] = patchStatus;
    result["divergentPath"] = persistedDivergentPath(
        divergentPath, divergentRepo
        );
    result["divergentPathType"] = divergentPathType;
    result["classification"] = classification;
    if (classification == "ERROR") {
        const QString savedReason = errorReason.trimmed().isEmpty()
            ? defaultErrorReasonForPatchStatus(patchStatus)
            : errorReason.trimmed();
        result["errorReason"] = savedReason;
    }
    result["similarityChecks"] = similarityLinesToJson(similarityLines);
    if (!hunkClassifications.isEmpty()) {
        result["hunks"] = hunkClassificationsToJson(hunkClassifications);
    }

    const QStringList order = {
        "fileName",
        "fileExtension",
        "language",
        "isConfigurationLanguage",
        "pr",
        "mainline",
        "divergentRepo",
        "changeStatus",
        "renamed",
        "renamedFileName",
        "patchAvailable",
        "patchStatus",
        "divergentPath",
        "divergentPathType",
        "classification",
        "errorReason",
        "similarityChecks",
        "hunks"
    };

    return writeOrderedJsonObjectFile(
        path,
        result,
        "per-file JSON result",
        order
        );
}

QJsonObject makePrFileResult(const QString &fileName,
                             const QString &classification,
                             bool patchAvailable,
                             const QString &patchStatus,
                             const QStringList &similarityLines,
                             const QVector<HunkClassificationInfo> &hunkClassifications,
                             const QString &errorReason){
    QJsonObject result;
    result["fileName"] = fileName;
    addFileLanguageMetadata(result, fileName);
    result["patchAvailable"] = patchAvailable;
    result["patchStatus"] = patchStatus;
    result["classification"] = classification;
    if (classification == "ERROR") {
        const QString savedReason = errorReason.trimmed().isEmpty()
            ? defaultErrorReasonForPatchStatus(patchStatus)
            : errorReason.trimmed();
        result["errorReason"] = savedReason;
    }
    result["similarityChecks"] = similarityLinesToJson(similarityLines);
    if (!hunkClassifications.isEmpty()) {
        result["hunks"] = hunkClassificationsToJson(hunkClassifications);
    }
    return result;
}
