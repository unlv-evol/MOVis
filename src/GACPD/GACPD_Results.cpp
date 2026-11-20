// GACPD stage 6: writing the run out.
//
// Results are produced at three levels: one results.json per classified file,
// one pr_results.json per pull request, and the CSV audits for the run as a
// whole. Files are also moved into a folder named after their classification
// so the results viewer can find them by browsing.

#include "headers/GACPD/GACPD.h"

#include "headers/GACPD/Internal/FetchedPrData.h"
#include "headers/GACPD/Internal/FileLanguages.h"
#include "headers/GACPD/Internal/GitHubRequests.h"
#include "headers/GACPD/Internal/JscpdEvidence.h"
#include "headers/GACPD/Internal/OrderedJson.h"
#include "headers/GACPD/Internal/ResultRecords.h"
#include "headers/GACPD/Methods/helpers_GACPD.h"
#include "headers/GACPD/Methods/common.h"
#include "headers/GACPD/Methods/patchExtractor.h"
#include "headers/GACPD/Methods/appPath.h"

#include <QCryptographicHash>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QTemporaryDir>
#include <exception>
#include <stdexcept>

QMap<QString, QVariantMap> GACPD::finalClass(const QMap<QString, QVariantMap>& resultDict){
    QMap<QString, QVariantMap> prClasses;

    for (QMap<QString, QVariantMap>::const_iterator prIt = resultDict.constBegin(); prIt != resultDict.constEnd(); prIt++) {
        const QString pr = prIt.key();
        const QVariantMap filesMap = prIt.value();

        int total_ED = 0;
        int total_MO = 0;
        int total_SP = 0;
        int total_NE = 0;
        int total_OE = 0;
        int total_E = 0;
        int total_NA = 0;
        int total_ERROR = 0;

        for (QVariantMap::const_iterator fileIt = filesMap.constBegin(); fileIt != filesMap.constEnd(); fileIt++) {
            const QVariantMap fileMap = fileIt.value().toMap();

            if (fileMap.contains("result")) {
                const QVariantMap resultMap = fileMap.value("result").toMap();
                const QString class_ = resultMap.value("patchClass").toString();

                if (class_ == "OTHER EXT") {
                    total_OE += 1;
                } else if (class_ == "NOT EXISTING") {
                    total_NE += 1;
                } else if (class_ == "MO") {
                    total_MO += 1;
                } else if (class_ == "ED") {
                    total_ED += 1;
                } else if (class_ == "SP") {
                    total_SP += 1;
                } else if (class_ == "NA" || class_ == "NA ADDED") {
                    total_NA += 1;
                } else if (class_ == "ERROR") {
                    total_ERROR += 1;
                }
            } else {
                total_ERROR += 1;
            }

            total_E += 1;
        }

        QString ultimateClass;

        if (total_MO == 0 && total_ED == 0) {
            ultimateClass = "NA";

            int checkNum = total_NA;

            if (checkNum < total_OE) {
                ultimateClass = "CC";
                checkNum = total_OE;
            }

            if (checkNum < total_NE) {
                ultimateClass = "NE";
                checkNum = total_NE;
            }

            if (checkNum < total_ERROR) {
                ultimateClass = "ERROR";
            }
        } else {
            if (total_MO != 0) {
                ultimateClass = "MO";
            } else if (total_ED != 0) {
                ultimateClass = "ED";
            } else {
                ultimateClass = "NA";
            }
        }

        QVariantMap totals;
        totals["total_ED"] = total_ED;
        totals["total_MO"] = total_MO;
        totals["total_SP"] = total_SP;
        totals["total_NE"] = total_NE;
        totals["total_OE"] = total_OE;
        totals["total_E"] = total_E;
        totals["total_NA"] = total_NA;
        totals["total_ERROR"] = total_ERROR;

        QVariantMap prData;
        prData["totals"] = totals;
        prData["class"] = ultimateClass;

        prClasses[pr] = prData;
    }

    return prClasses;
}

void GACPD::writePrHeader(QJsonObject &out, const QString &prNr){
    const QVariantMap pr = repoData.value(prNr);

    out["classifiedPR"] = prNr;
    out["prTitle"] = pr["title"].toString();
    out["prDescription"] = pr["body"].toString();
    out["prLocation"] = pr["html_url"].toString();
    out["repoDivergenceDate"] = divergenceDate;
    out["cutoffDate"] = cutOffDate;
}

void GACPD::writeGitChecks(QJsonObject &out, const gitFiles &gitChecks){
    QJsonArray addedFiles;
    for (const QString &file : gitChecks.addedFiles) {
        addedFiles.append(file);
    }
    out["addedFilesSkipped"] = addedFiles;

    QJsonArray renamedFiles;
    if (!gitChecks.renamedFileRecords.isEmpty()) {
        for (const QPair<QString, QString> &renameRecord : gitChecks.renamedFileRecords) {
            QJsonObject rename;
            rename["from"] = renameRecord.first;
            rename["to"] = renameRecord.second;
            renamedFiles.append(rename);
        }
    } else {
        // Backward-compatible fallback for callers that only populate the
        // lookup map (for example older/local git-diff code paths).
        for (QMap<QString, QString>::const_iterator it = gitChecks.renamedFiles.constBegin();
             it != gitChecks.renamedFiles.constEnd();
             it++) {
            QJsonObject rename;
            rename["from"] = it.key();
            rename["to"] = it.value();
            renamedFiles.append(rename);
        }
    }
    out["renamedFiles"] = renamedFiles;
}

QMap<QString, QString> GACPD::buildReverseRenameMap(
    const QMap<QString, QString> &renamedFiles){
    QMap<QString, QString> reverseMap;

    for (QMap<QString, QString>::const_iterator it = renamedFiles.constBegin(); it != renamedFiles.constEnd(); it++) {
        reverseMap[it.value()] = it.key();
    }

    return reverseMap;
}

void GACPD::initializeFileResult(
    const QString &prNr,
    const QString &file){
    QMutexLocker locker(&repoResultsMutex);

    QVariantMap prResults = repoResults.value(prNr);
    prResults[file] = QVariantMap{};
    repoResults[prNr] = prResults;
}

void GACPD::saveFinalFileResult(
    const QString &prNr,
    const QString &file,
    const QString &status,
    const QString &destPath,
    const QString &patchPath,
    const QString &classification,
    const QVector<HunkClassificationInfo> &hunkClassifications){
    QVariantMap resultMod;
    resultMod["type"] = status.toUpper();
    resultMod["destPath"] = destPath;
    resultMod["patchPath"] = patchPath;
    resultMod["patchClass"] = classification;

    QVariantMap finalFileResult;
    finalFileResult["result"] = resultMod;

    QVariantList hunkResults;
    for (const HunkClassificationInfo &hunk : hunkClassifications) {
        QVariantMap hunkMap;
        hunkMap["number"] = hunk.hunkNumber;
        hunkMap["classification"] = hunk.classification;
        hunkMap["additionsClassification"] = hunk.additionsClassification;
        hunkMap["deletionsClassification"] = hunk.deletionsClassification;
        hunkResults.append(hunkMap);
    }
    finalFileResult["hunks"] = hunkResults;

    QMutexLocker locker(&repoResultsMutex);

    QVariantMap prResults = repoResults.value(prNr);
    prResults[file] = finalFileResult;
    repoResults[prNr] = prResults;
}

void GACPD::markFileAsOtherExtension(
    const QString &prNr,
    const QString &file,
    const QString &fileResultsDir,
    const QString &safeFileName){
    QVariantMap resultMod;
    resultMod["patchClass"] = "OTHER EXT";

    QVariantMap updatedFileResult;
    updatedFileResult["results"] = QVariantList{};
    updatedFileResult["result"] = resultMod;

    {
        QMutexLocker locker(&repoResultsMutex);
        QVariantMap prResults = repoResults.value(prNr);
        prResults[file] = updatedFileResult;
        repoResults[prNr] = prResults;
    }

    // Unsupported/invalid formats are classified as OTHER EXT in the
    // results data, but their artifacts belong under the CC directory.
    // Use the same robust mover as normal classifications instead of
    // QDir(fileResultsDir).rename(...), which resolves the supplied path
    // relative to fileResultsDir and can silently fail on Windows.
    moveFileResultToClassificationFolder(
        prNr,
        safeFileName,
        fileResultsDir,
        "CC"
        );
}

void GACPD::markFileAsNotExisting(
    const QString &prNr,
    const QString &file,
    const QString &fileResultsDir,
    const QString &safeFileName){
    QVariantMap resultMod;
    resultMod["patchClass"] = "NOT EXISTING";

    QVariantMap updatedFileResult;
    updatedFileResult["results"] = QVariantList{};
    updatedFileResult["result"] = resultMod;

    {
        QMutexLocker locker(&repoResultsMutex);
        QVariantMap prResults = repoResults.value(prNr);
        prResults[file] = updatedFileResult;
        repoResults[prNr] = prResults;
    }

    const QString neDir =
        movisResultsPath(rootFolder + "/" + prNr + "/NE/" + safeFileName);

    QDir().mkpath(QFileInfo(neDir).absolutePath());
    copyDirectory(fileResultsDir, neDir);
    QDir(fileResultsDir).removeRecursively();
}

void GACPD::markFileAsAddedNA(
    const QString &prNr,
    const QString &file,
    const QString &status,
    const QString &patchPath,
    const QString &fileResultsDir,
    const QString &safeFileName){
    QVariantMap resultMod;
    resultMod["type"] = status.toUpper();
    resultMod["destPath"] = "";
    resultMod["patchPath"] = patchPath;
    resultMod["patchClass"] = "NA";

    QVariantMap updatedFileResult;
    updatedFileResult["result"] = resultMod;

    {
        QMutexLocker locker(&repoResultsMutex);
        QVariantMap prResults = repoResults.value(prNr);
        prResults[file] = updatedFileResult;
        repoResults[prNr] = prResults;
    }

    // Keep added-file artifacts grouped just like the other file-level
    // classifications. This also avoids the same relative-path rename issue
    // that affected CC/OTHER EXT files.
    moveFileResultToClassificationFolder(
        prNr,
        safeFileName,
        fileResultsDir,
        "NA_ADDED"
        );
}

void GACPD::handleFileClassificationError(
    const QString &prNr,
    const QString &file,
    const QString &fileResultsDir,
    const QString &safeFileName,
    const QString &errorReason){
    #ifdef QT_DEBUG
        qDebug() << "Exception thrown while processing file:" << file;
    #endif

    const QString savedReason = errorReason.trimmed().isEmpty()
        ? QStringLiteral(
            "A file-processing error occurred before a complete result could be saved. See the application log for additional details."
            )
        : errorReason.trimmed();

    QVariantMap resultMod;
    resultMod["patchClass"] = "ERROR";
    resultMod["errorReason"] = savedReason;

    QVariantMap errorFileResult;
    errorFileResult["result"] = resultMod;

    {
        QMutexLocker locker(&repoResultsMutex);
        QVariantMap prResults = repoResults.value(prNr);
        prResults[file] = errorFileResult;
        repoResults[prNr] = prResults;
    }

    const QString errorDir =
        movisResultsPath(rootFolder + "/" + prNr + "/ERROR/" + safeFileName);

    QDir().mkpath(QFileInfo(errorDir).absolutePath());
    copyDirectory(fileResultsDir, errorDir);
    QDir(fileResultsDir).removeRecursively();

    // If the original failure was itself a results.json write failure, there
    // may be no record for the viewer to open. Preserve a minimal diagnostic
    // record in the ERROR folder so the failure is still inspectable.
    const QString errorResultsPath = QDir(errorDir).filePath("results.json");
    if (!QFileInfo::exists(errorResultsPath)) {
        QJsonObject diagnostic;
        diagnostic["fileName"] = file;
        addFileLanguageMetadata(diagnostic, file);
        diagnostic["pr"] = prNr;
        diagnostic["classification"] = "ERROR";
        diagnostic["errorReason"] = savedReason;

        const QStringList fieldOrder = {
            "fileName",
            "fileExtension",
            "language",
            "isConfigurationLanguage",
            "pr",
            "classification",
            "errorReason"
        };

        if (!writeOrderedJsonObjectFile(
                errorResultsPath,
                diagnostic,
                "fallback ERROR JSON result",
                fieldOrder)) {
            qWarning() << "Could not save fallback ERROR results.json for"
                       << file << "in PR" << prNr;
        }
    }
}

void GACPD::writeSingleFileResults(
    const QString &prNr,
    const QString &file,
    const QString &changeStatus,
    const QString &destPath,
    bool renamedDestPath,
    bool prRenamed,
    const QString &renamedFileName,
    bool patchAvailable,
    const QString &patchStatus,
    const ClassificationResult &classificationResult,
    const QString &safeFileName,
    QJsonArray &prOut){
    const QString resultsFilePath =
        movisResultsPath(rootFolder + "/" + prNr + "/" + safeFileName + "/results.json");

    const bool jsonSaved = writeIndividualFileResultJson(
        resultsFilePath,
        file,
        prNr,
        sourceRepo,
        variantRepo,
        changeStatus,
        prRenamed,
        renamedFileName,
        destPath,
        renamedDestPath ? "renamed" : "resolved",
        classificationResult.classification,
        patchAvailable,
        patchStatus,
        classificationResult.similarityLines,
        classificationResult.hunkClassifications
        );

    if (!jsonSaved) {
        throw std::runtime_error(
            QString("Failed to save classified-file results JSON: %1")
                .arg(resultsFilePath).toStdString()
            );
    }

    prOut.append(
        makePrFileResult(
            file,
            classificationResult.classification,
            patchAvailable,
            patchStatus,
            classificationResult.similarityLines,
            classificationResult.hunkClassifications
            )
        );
}

void GACPD::moveFileResultToClassificationFolder(
    const QString &prNr,
    const QString &safeFileName,
    const QString &fileResultsDir,
    const QString &classification){
    const QString classificationDir =
        movisResultsPath(rootFolder + "/" + prNr + "/" + classification + "/" + safeFileName);

    QDir().mkpath(QFileInfo(classificationDir).absolutePath());

    // Use a neutral QDir so the source/destination paths are interpreted
    // from the application working directory. Constructing QDir with
    // fileResultsDir and then passing fileResultsDir again makes Qt treat the
    // old name as relative to that directory, which can make rename fail.
    QDir dir;

    if (!dir.rename(fileResultsDir, classificationDir)) {
        // Rename can still fail (for example because of platform/filesystem
        // restrictions). Fall back to copy + remove, but only remove the
        // original after a successful copy so no results are lost.
        if (copyDirectory(fileResultsDir, classificationDir)) {
            QDir(fileResultsDir).removeRecursively();
        } else {
            qWarning() << "Failed to move classification results from"
                       << fileResultsDir << "to" << classificationDir;
        }
    }
}

static QString csvField(QString value) {
    value.replace('"', "\"\"");

    if (value.contains(',') || value.contains('"') ||
        value.contains('\n') || value.contains('\r')) {
        return QString("\"%1\"").arg(value);
    }

    return value;
}

void GACPD::writeClassificationAudits() {
    const QString auditDir = movisResultsPath(rootFolder);

    if (!QDir().mkpath(auditDir)) {
        qWarning() << "Could not create audit directory:" << auditDir;
        return;
    }

    QStringList prs = repoData.keys();
    std::sort(prs.begin(), prs.end(), [](const QString &left, const QString &right) {
        bool leftOk = false;
        bool rightOk = false;
        const qlonglong leftNumber = left.toLongLong(&leftOk);
        const qlonglong rightNumber = right.toLongLong(&rightOk);

        if (leftOk && rightOk && leftNumber != rightNumber) {
            return leftNumber < rightNumber;
        }

        return left < right;
    });

    QMap<QString, QVariantMap> resultsSnapshot;
    {
        QMutexLocker locker(&repoResultsMutex);
        resultsSnapshot = repoResults;
    }

    QFile classificationAudit(auditDir + "/classification_audit.csv");
    if (classificationAudit.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&classificationAudit);
        out.setEncoding(QStringConverter::Utf8);
        out << "PR,PR_Classification,Files_Fetched_total,Hunks_Fetched_total\n";

        qint64 totalFiles = 0;
        qint64 totalHunks = 0;

        for (const QString &prNr : prs) {
            const int fileCount = countFilesInPr(prNr);
            const int hunkCount = countHunksInPr(prNr);
            const QString prClassification =
                prClassifications.value(prNr).value("class").toString();
            const int resultEntries = resultsSnapshot.value(prNr).size();

            // Keep the old integrity check as a runtime diagnostic even
            // though the requested CSV schema no longer contains the
            // result_entries/match columns.
            if (fileCount != resultEntries) {
                qCritical() << "Classification file-count mismatch for PR"
                            << prNr
                            << "fetched:" << fileCount
                            << "results:" << resultEntries;
            }

            totalFiles += fileCount;
            totalHunks += hunkCount;

            out << csvField(prNr) << ','
                << csvField(prClassification) << ','
                << fileCount << ','
                << hunkCount << '\n';
        }

        qInfo() << "Classification audit totals - files:"
                << totalFiles << "hunks:" << totalHunks;
    } else {
        qWarning() << "Could not write classification audit:"
                   << classificationAudit.fileName();
    }

    QFile fileAudit(auditDir + "/file_audit.csv");
    if (fileAudit.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&fileAudit);
        out.setEncoding(QStringConverter::Utf8);
        out << "file_name,file_classification,file_extensions,pr_of_file\n";

        for (const QString &prNr : prs) {
            QStringList files;
            const QVariantList commitsData =
                repoData.value(prNr).value("commits_data").toList();

            QSet<QString> uniqueFiles;
            for (const QVariant &filesVariant : commitsData) {
                const QVariantMap fileMap = filesVariant.toMap();
                for (QVariantMap::const_iterator it = fileMap.constBegin(); it != fileMap.constEnd(); ++it) {
                    uniqueFiles.insert(it.key());
                }
            }

            files = uniqueFiles.values();
            std::sort(files.begin(), files.end());

            const QVariantMap prResults = resultsSnapshot.value(prNr);

            for (const QString &file : files) {
                const QVariantMap fileResult = prResults.value(file).toMap();
                const QVariantMap resultMap = fileResult.value("result").toMap();
                const QString classification =
                    resultMap.value("patchClass").toString();
                const FileExtensionInfo extInfo = detectFileExtension(file);

                out << csvField(file) << ','
                    << csvField(classification) << ','
                    << csvField(extInfo.extension) << ','
                    << csvField(prNr) << '\n';
            }
        }
    } else {
        qWarning() << "Could not write file audit:" << fileAudit.fileName();
    }

    QFile hunkAudit(auditDir + "/hunk_audit.csv");
    if (hunkAudit.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&hunkAudit);
        out.setEncoding(QStringConverter::Utf8);
        out << "hunk_number,hunk_classification,file_of_hunk,pr_of_hunk\n";

        for (const QString &prNr : prs) {
            const QVariantMap prResults = resultsSnapshot.value(prNr);
            const QVariantList commitsData =
                repoData.value(prNr).value("commits_data").toList();

            for (const QVariant &filesVariant : commitsData) {
                const QVariantMap fileMap = filesVariant.toMap();
                QStringList files = fileMap.keys();
                std::sort(files.begin(), files.end());

                for (const QString &file : files) {
                    const QVariantList fileEntries = fileMap.value(file).toList();
                    if (fileEntries.isEmpty()) {
                        continue;
                    }

                    const QString patch =
                        fileEntries.first().toMap().value("patch").toString();
                    const int hunkCount = countHunksInPatch(patch);

                    const QVariantMap fileResult = prResults.value(file).toMap();
                    const QVariantMap resultMap = fileResult.value("result").toMap();
                    const QString fileClassification =
                        resultMap.value("patchClass").toString();
                    const QVariantList savedHunks = fileResult.value("hunks").toList();

                    QMap<int, QString> hunkClasses;
                    for (const QVariant &hunkVariant : savedHunks) {
                        const QVariantMap hunkMap = hunkVariant.toMap();
                        hunkClasses[hunkMap.value("number").toInt()] =
                            hunkMap.value("classification").toString();
                    }

                    for (int hunkNumber = 1; hunkNumber <= hunkCount; ++hunkNumber) {
                        QString hunkClassification = hunkClasses.value(hunkNumber);

                        // Files that cannot enter similarity analysis (for
                        // example OTHER EXT, NOT EXISTING, NA-added, or ERROR)
                        // still have fetched patch hunks. In those cases the
                        // file-level terminal classification is the most
                        // accurate hunk-level status available.
                        if (hunkClassification.isEmpty()) {
                            hunkClassification = fileClassification;
                        }

                        out << hunkNumber << ','
                            << csvField(hunkClassification) << ','
                            << csvField(file) << ','
                            << csvField(prNr) << '\n';
                    }
                }
            }
        }
    } else {
        qWarning() << "Could not write hunk audit:" << hunkAudit.fileName();
    }
}

void GACPD::finalizeResults(){
    #ifdef QT_DEBUG
        qDebug().noquote()
        << QString("\nBar plot of the patch classifications for %1 -> %2")
            .arg(sourceRepo, variantRepo);
    #endif

    int total_NA = 0;
    int total_ED = 0;
    int total_MO = 0;
    int total_CC = 0;
    int total_SP = 0;
    int total_NE = 0;
    int total_ERROR = 0;

    int total_all = 0;
    int total_mo_all = 0;
    int total_ed_all = 0;
    int total_sp_all = 0;
    int total_na_all = 0;

    for (QMap<QString, QVariantMap>::const_iterator it = prClassifications.constBegin(); it != prClassifications.constEnd(); it++) {
        const QString pr = it.key();

        const QVariantMap data = it.value();
        const QString class_ = data.value("class").toString();

        const QString baseDir = movisResultsPath(rootFolder);

        const QString oldPath =
            QString("%1/%2").arg(baseDir, pr);

        const QString newPath =
            QString("%1/%2_%3").arg(baseDir, pr, class_);

        QDir newDir(newPath);

        if (newDir.exists()) {
            if (!newDir.removeRecursively()) {
                throw std::runtime_error(
                    QString("Failed to remove existing destination: %1")
                        .arg(newPath)
                        .toStdString()
                    );
            }
        }

        QFileInfo srcInfo(oldPath);
        QFileInfo dstInfo(newPath);

        if (!srcInfo.exists()) {
            throw std::runtime_error(
                QString("Source does not exist: %1")
                    .arg(oldPath)
                    .toStdString()
                );
        }

        if (dstInfo.exists()) {
            throw std::runtime_error(
                QString("Destination already exists: %1")
                    .arg(newPath)
                    .toStdString()
                );
        }

        for (int i = 0; i < 3; i++) {
            QDir parentDir(baseDir);

            if (parentDir.rename(pr, QString("%1_%2").arg(pr, class_))) {
                break;
            }

            if (i == 2) {
                throw std::runtime_error(
                    QString("Failed to rename directory from %1 to %2")
                        .arg(oldPath, newPath)
                        .toStdString()
                    );
            }

            QThread::msleep(500);
        }

        if (class_ == "ED") {
            total_ED += 1;
        } else if (class_ == "MO") {
            total_MO += 1;
        } else if (class_ == "SP") {
            total_SP += 1;
        } else if (class_ == "NA") {
            total_NA += 1;
        } else if (class_ == "CC") {
            total_CC += 1;
        } else if (class_ == "NE") {
            total_NE += 1;
        } else if (class_ == "ERROR") {
            total_ERROR += 1;
        }

        const int total_mid = total_MO + total_ED + total_SP;
        total_all += total_mid;
    }

    const int total_total = pullRequests.size();

    total_mo_all += total_MO;
    total_ed_all += total_ED;
    total_sp_all += total_SP;
    total_na_all += total_NA;

    QList<int> totalsList = {
        total_MO,
        total_ED,
        total_SP,
        total_CC,
        total_NE,
        total_NA,
        total_ERROR
    };

    // TODO:
    // analysis.all_class_bar(
    //     totalsList,
    //     repoCheckNumber,
    //     repoMainLine,
    //     repoDivergent,
    //     false
    // );

    Q_UNUSED(total_all);
    Q_UNUSED(total_mo_all);
    Q_UNUSED(total_ed_all);
    Q_UNUSED(total_sp_all);
    Q_UNUSED(total_na_all);
    Q_UNUSED(total_total);
    Q_UNUSED(totalsList);
}
