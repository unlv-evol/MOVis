// GACPD stage 5: deciding what happened to each changed file.
//
// Every file is copied into an isolated workspace next to the version it is
// compared against, jscpd is run over that workspace at descending token
// thresholds, and the clones it reports decide the classification. Files that
// cannot be compared at all are marked here too.

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

void GACPD::processPrCommitFiles(
    const QString& prNr,
    const gitFiles& gitChecks,
    const QMap<QString, QString>& reverseRenameMap,
    QJsonArray& out) {
    const QVariantList commitsData = repoData[prNr]["commits_data"].toList();

    int totalFiles = countFilesInPr(prNr);
    int doneFiles = 0;

    QSet<QString> processedFiles;

    for (const QVariant& filesVariant : commitsData) {
        const QVariantMap files = filesVariant.toMap();

        for (QVariantMap::const_iterator it = files.constBegin(); it != files.constEnd(); it++) {
            const QString file = it.key();
            const QVariantList fileEntries = it.value().toList();

            processSingleFile(
                prNr,
                file,
                fileEntries,
                gitChecks,
                reverseRenameMap,
                out
                );

            if (!processedFiles.contains(file)) {
                processedFiles.insert(file);
                doneFiles++;
                emit prFileProgress(prNr, doneFiles, totalFiles);
            }
        }
    }
}

void GACPD::processSingleFile(
    const QString &prNr,
    const QString &file,
    const QVariantList &fileEntries,
    const gitFiles &gitChecks,
    const QMap<QString, QString> &reverseRenameMap,
    QJsonArray &out){
    const QString safeFileName = safeName(file);

    const QString fileResultsDir =
        movisResultsPath(rootFolder + "/" + prNr + "/" + safeFileName);

    QDir().mkpath(fileResultsDir);

    initializeFileResult(prNr, file);

    const QVariantMap firstEntry = fileEntries.isEmpty()
        ? QVariantMap{}
        : fileEntries.first().toMap();
    const bool patchAvailable =
        !fileEntries.isEmpty() && fetchedPatchAvailable(firstEntry);
    const QString patchStatus = fileEntries.isEmpty()
        ? QStringLiteral("missing_file_entry")
        : fetchedPatchStatus(firstEntry);

    const QString changeStatus = firstEntry.value("status").toString();
    const bool prRenamed =
        changeStatus.compare("renamed", Qt::CaseInsensitive) == 0 ||
        reverseRenameMap.contains(file);
    const QString renamedFileName = prRenamed ? file : QString();
    const QString resultsFilePath = fileResultsDir + "/results.json";
    const int githubChanges = firstEntry.value("changes").toInt();

    // Terminal classifications must never walk the repository-wide historical
    // rename graph. The data-integrity work originally moved full rename-path
    // resolution here so divergentPath would not be empty on early exits. For
    // rename-only PRs that meant a file with no patch could spend an
    // unbounded/very long time resolving historical names before reaching the
    // zero-change return. Preserve divergentPath with a cheap PR-local lookup
    // instead; full historical resolution is reserved for files that actually
    // enter similarity classification.
    const QString metadataFileName = file_Name(file);
    const QStringList metadataFileDir = file_Dir(file);
    QString metadataNewFileDir;
    for (const QString &part : metadataFileDir) {
        metadataNewFileDir += part + "/";
    }

    const QString metadataRelativeFile =
        metadataNewFileDir + metadataFileName;
    bool metadataRenamedDestPath = false;
    QString metadataDivergentPath = buildDivergentFilePath(
        metadataNewFileDir,
        metadataFileName
        );

    const QFileInfo currentMetadataFile(metadataDivergentPath);
    if ((!currentMetadataFile.exists() || !currentMetadataFile.isFile()) &&
        prRenamed) {
        QString previousFileName =
            firstEntry.value("previous_filename").toString().trimmed();

        if (previousFileName.isEmpty()) {
            previousFileName = reverseRenameMap.value(file).trimmed();
        }

        if (!previousFileName.isEmpty()) {
            metadataDivergentPath = movisWorkingPath(
                "Results/Repos_files/" +
                rootFolder + "/" +
                variantRepo + "/" +
                previousFileName
                );
            metadataRenamedDestPath =
                normalizePath(previousFileName) !=
                normalizePath(metadataRelativeFile);
        }
    }

    const QString metadataDivergentPathType =
        metadataRenamedDestPath ? QStringLiteral("renamed")
                                : QStringLiteral("resolved");

    const FileExtensionInfo extInfo = detectFileExtension(file);

    // GitHub explicitly reporting zero changes is the authoritative fast path
    // for rename-only/no-text PR files. Do this before creating patch/src/cmp
    // workspaces, before patch archival, and before historical rename lookup.
    // This is what keeps "File renamed without changes" from sitting at 0/1.
    if (!fileEntries.isEmpty() && extInfo.isSupported && githubChanges == 0) {
        const QString zeroPatchStatus =
            QStringLiteral("not_provided_no_textual_changes");

        #ifdef QT_DEBUG
            qDebug() << "Zero textual changes for" << file << "in PR" << prNr
                     << "(status:" << changeStatus << ") - classifying as NA"
                     << "without patch processing or JSCPD";
        #endif

        const bool jsonSaved = writeIndividualFileResultJson(
            resultsFilePath,
            file,
            prNr,
            sourceRepo,
            variantRepo,
            changeStatus,
            prRenamed,
            renamedFileName,
            metadataDivergentPath,
            metadataDivergentPathType,
            "NA",
            false,
            zeroPatchStatus
            );

        if (!jsonSaved) {
            qCritical() << "Failed to save zero-change NA results.json for"
                        << file << "in PR" << prNr;
            const QString errorReason = QString(
                "Could not save or verify the zero-change results.json for %1."
                ).arg(file);
            handleFileClassificationError(
                prNr, file, fileResultsDir, safeFileName, errorReason
                );
            out.append(
                makePrFileResult(
                    file, "ERROR", false, zeroPatchStatus, QStringList(), QVector<HunkClassificationInfo>{}, errorReason
                    )
                );
            return;
        }

        saveFinalFileResult(
            prNr,
            file,
            changeStatus,
            metadataDivergentPath,
            QString(),
            "NA",
            QVector<HunkClassificationInfo>{}
            );
        out.append(makePrFileResult(file, "NA", false, zeroPatchStatus));
        moveFileResultToClassificationFolder(
            prNr, safeFileName, fileResultsDir, "NA"
            );
        return;
    }

    // A supported source file with reported textual changes but no GitHub
    // patch is incomplete research input. Fail it immediately rather than
    // manufacturing an empty patch and entering the hunk pipeline.
    if (!fileEntries.isEmpty() && extInfo.isSupported &&
        !patchAvailable && githubChanges > 0) {
        const QString errorReason = QString(
            "GitHub reported %1 textual changes, but no usable patch text was available for classification."
            ).arg(githubChanges);
        const bool jsonSaved = writeIndividualFileResultJson(
            resultsFilePath,
            file,
            prNr,
            sourceRepo,
            variantRepo,
            changeStatus,
            prRenamed,
            renamedFileName,
            metadataDivergentPath,
            metadataDivergentPathType,
            "ERROR",
            false,
            patchStatus,
            QStringList(),
            {},
            errorReason
            );

        if (!jsonSaved) {
            qCritical() << "Failed to save missing-patch ERROR results.json for"
                        << file << "in PR" << prNr;
        }

        handleFileClassificationError(
            prNr, file, fileResultsDir, safeFileName, errorReason
            );
        out.append(makePrFileResult(file, "ERROR", false, patchStatus, QStringList(), QVector<HunkClassificationInfo>{}, errorReason));
        return;
    }

    // Preserve the exact fetched patch inside the file's result workspace.
    // A write failure is a data-integrity failure, not a best-effort warning:
    // do not continue and later count the file as successfully completed.
    const ClassificationWorkspace workspace =
        createWorkspace(prNr, safeFileName);

    if (patchAvailable) {
        const QString patchContents = firstEntry.value("patch").toString();
        const QString patchFileName = safeName(file_Name(file));
        int dupCount = 0;

        try {
            savePatch(
                workspace.patch,
                patchFileName,
                patchContents,
                dupCount
                );
        } catch (const std::exception &e) {
            const QString storageStatus =
                QStringLiteral("available_but_archive_write_failed");
            const QString errorReason = QString(
                "Could not preserve the fetched patch in the result workspace: %1"
                ).arg(QString::fromUtf8(e.what()));
            qCritical() << "Could not preserve required archival patch for"
                        << file << "in PR" << prNr << ":" << e.what();

            const bool errorJsonSaved = writeIndividualFileResultJson(
                resultsFilePath,
                file,
                prNr,
                sourceRepo,
                variantRepo,
                changeStatus,
                prRenamed,
                renamedFileName,
                metadataDivergentPath,
                metadataDivergentPathType,
                "ERROR",
                true,
                storageStatus,
                QStringList(),
                {},
                errorReason
                );
            if (!errorJsonSaved) {
                qCritical() << "Also failed to save ERROR results.json for"
                            << file << "in PR" << prNr;
            }

            handleFileClassificationError(
                prNr, file, fileResultsDir, safeFileName, errorReason
                );
            out.append(
                makePrFileResult(file, "ERROR", true, storageStatus, QStringList(), QVector<HunkClassificationInfo>{}, errorReason)
                );
            return;
        }
    }

    if (!extInfo.isSupported) {
        const bool jsonSaved = writeIndividualFileResultJson(
            resultsFilePath,
            file,
            prNr,
            sourceRepo,
            variantRepo,
            changeStatus,
            prRenamed,
            renamedFileName,
            metadataDivergentPath,
            metadataDivergentPathType,
            "OTHER EXT",
            patchAvailable,
            patchStatus
            );

        if (!jsonSaved) {
            qCritical() << "Failed to save required results.json for"
                        << file << "in PR" << prNr;
            const QString errorReason = QString(
                "Could not save or verify results.json while recording an unsupported file format: %1."
                ).arg(file);
            handleFileClassificationError(
                prNr, file, fileResultsDir, safeFileName, errorReason
                );
            out.append(
                makePrFileResult(
                    file, "ERROR", patchAvailable, patchStatus, QStringList(), QVector<HunkClassificationInfo>{}, errorReason
                    )
                );
            return;
        }

        markFileAsOtherExtension(prNr, file, fileResultsDir, safeFileName);
        out.append(
            makePrFileResult(file, "OTHER EXT", patchAvailable, patchStatus)
            );
        return;
    }

    if (fileEntries.isEmpty()) {
        const bool jsonSaved = writeIndividualFileResultJson(
            resultsFilePath,
            file,
            prNr,
            sourceRepo,
            variantRepo,
            changeStatus,
            prRenamed,
            renamedFileName,
            metadataDivergentPath,
            metadataDivergentPathType,
            "NOT EXISTING",
            false,
            patchStatus
            );

        if (!jsonSaved) {
            qCritical() << "Failed to save required results.json for"
                        << file << "in PR" << prNr;
            const QString errorReason = QString(
                "Could not save or verify the NOT EXISTING results.json for %1."
                ).arg(file);
            handleFileClassificationError(
                prNr, file, fileResultsDir, safeFileName, errorReason
                );
            out.append(
                makePrFileResult(
                    file, "ERROR", false, patchStatus, QStringList(), QVector<HunkClassificationInfo>{}, errorReason
                    )
                );
            return;
        }

        markFileAsNotExisting(prNr, file, fileResultsDir, safeFileName);
        out.append(
            makePrFileResult(file, "NOT EXISTING", false, patchStatus)
            );
        return;
    }

    try {
        classifyExistingFile(
            prNr,
            file,
            fileEntries,
            gitChecks,
            reverseRenameMap,
            fileResultsDir,
            safeFileName,
            extInfo,
            out
            );
    } catch (const std::exception &e) {
        qCritical() << "File classification failed for" << file
                    << "in PR" << prNr << ":" << e.what();

        const QString errorReason =
            QString("File classification failed: %1")
                .arg(QString::fromUtf8(e.what()));

        const bool errorJsonSaved = writeIndividualFileResultJson(
            resultsFilePath,
            file,
            prNr,
            sourceRepo,
            variantRepo,
            changeStatus,
            prRenamed,
            renamedFileName,
            metadataDivergentPath,
            metadataDivergentPathType,
            "ERROR",
            patchAvailable,
            patchStatus,
            QStringList(),
            {},
            errorReason
            );
        if (!errorJsonSaved) {
            qCritical() << "Also failed to save ERROR results.json for"
                        << file << "in PR" << prNr;
        }

        handleFileClassificationError(prNr, file, fileResultsDir, safeFileName, errorReason);
        out.append(
            makePrFileResult(file, "ERROR", patchAvailable, patchStatus, QStringList(), QVector<HunkClassificationInfo>{}, errorReason)
            );
    } catch (...) {
        qCritical() << "Unknown file classification failure for" << file
                    << "in PR" << prNr;

        const QString errorReason =
            QStringLiteral("File classification failed with an unknown exception.");

        const bool errorJsonSaved = writeIndividualFileResultJson(
            resultsFilePath,
            file,
            prNr,
            sourceRepo,
            variantRepo,
            changeStatus,
            prRenamed,
            renamedFileName,
            metadataDivergentPath,
            metadataDivergentPathType,
            "ERROR",
            patchAvailable,
            patchStatus,
            QStringList(),
            {},
            errorReason
            );
        if (!errorJsonSaved) {
            qCritical() << "Also failed to save ERROR results.json for"
                        << file << "in PR" << prNr;
        }

        handleFileClassificationError(prNr, file, fileResultsDir, safeFileName, errorReason);
        out.append(
            makePrFileResult(file, "ERROR", patchAvailable, patchStatus, QStringList(), QVector<HunkClassificationInfo>{}, errorReason)
            );
    }
}

FileExtensionInfo GACPD::detectFileExtension(const QString &file) const{
    return fileExtensionInfoFromInternalLists(file);
}

ClassificationWorkspace GACPD::createWorkspace(
    const QString &prNr,
    const QString &safeFileName){
    ClassificationWorkspace workspace;

    workspace.root =
        movisResultsPath(rootFolder + "/" + prNr + "/" + safeFileName);

    workspace.src = workspace.root + "/src";
    workspace.cmp = workspace.root + "/cmp";
    workspace.patch = workspace.root + "/patch";
    workspace.reports = workspace.root + "/reports";

    QDir().mkpath(workspace.src);
    QDir().mkpath(workspace.cmp);
    QDir().mkpath(workspace.patch);
    QDir().mkpath(workspace.reports);

    return workspace;
}

void GACPD::classifyExistingFile(
    const QString &prNr,
    const QString &file,
    const QVariantList &fileEntries,
    const gitFiles &gitChecks,
    const QMap<QString, QString> &reverseRenameMap,
    const QString &fileResultsDir,
    const QString &safeFileName,
    const FileExtensionInfo &extInfo,
    QJsonArray &out){
    const QVariantMap firstEntry = fileEntries.first().toMap();

    const QString fileName = file_Name(file);
    const QStringList fileDir = file_Dir(file);
    const QString status = firstEntry["status"].toString();
    const bool prRenamed =
        status.compare("renamed", Qt::CaseInsensitive) == 0 ||
        reverseRenameMap.contains(file);
    const QString renamedFileName = prRenamed ? file : QString();
    const QString resultsFilePath = fileResultsDir + "/results.json";
    const bool patchAvailable = fetchedPatchAvailable(firstEntry);
    const QString patchStatus = fetchedPatchStatus(firstEntry);
    const int githubChanges = firstEntry.value("changes").toInt();

    QString newFileDir;
    for (const QString &part : fileDir) {
        newFileDir += part + "/";
    }

    QString destPath = buildDivergentFilePath(newFileDir, fileName);

    const QString divergentFileNameForRenamesOrAdditions = newFileDir + fileName;
    const QString mainlineFileNameForRenamesOrAdditions =
        divergentFileNameForRenamesOrAdditions;

    const QString patchLines = firstEntry.value("patch").toString();

    QString patchPath = buildPatchDirectoryPath(prNr, newFileDir);

    const QString patchName = safeName(fileName);

    // Defensive terminal handling. processSingleFile() normally catches both
    // of these cases before calling this function, but keep this function safe
    // if it is called directly in the future. Crucially, do not perform the
    // repository-wide historical rename lookup when there is no patch to
    // classify.
    if (!patchAvailable) {
        bool terminalRenamedDestPath = false;
        QString terminalDestPath = destPath;

        if (prRenamed && reverseRenameMap.contains(
                mainlineFileNameForRenamesOrAdditions)) {
            const QString previousName = reverseRenameMap.value(
                mainlineFileNameForRenamesOrAdditions).trimmed();
            if (!previousName.isEmpty()) {
                terminalDestPath = movisWorkingPath(
                    "Results/Repos_files/" +
                    rootFolder + "/" +
                    variantRepo + "/" +
                    previousName
                    );
                terminalRenamedDestPath = true;
            }
        }

        const QString terminalPathType = terminalRenamedDestPath
            ? QStringLiteral("renamed")
            : QStringLiteral("resolved");
        const QString terminalClass = githubChanges == 0
            ? QStringLiteral("NA")
            : QStringLiteral("ERROR");
        const QString terminalPatchStatus = githubChanges == 0
            ? QStringLiteral("not_provided_no_textual_changes")
            : patchStatus;

        const bool jsonSaved = writeIndividualFileResultJson(
            resultsFilePath,
            file,
            prNr,
            sourceRepo,
            variantRepo,
            status,
            prRenamed,
            renamedFileName,
            terminalDestPath,
            terminalPathType,
            terminalClass,
            false,
            terminalPatchStatus
            );

        if (!jsonSaved) {
            throw std::runtime_error(
                QString("Failed to save terminal no-patch results JSON: %1")
                    .arg(resultsFilePath).toStdString()
                );
        }

        if (githubChanges == 0) {
            saveFinalFileResult(
                prNr, file, status, terminalDestPath, QString(), "NA",
                QVector<HunkClassificationInfo>{}
                );
            out.append(makePrFileResult(
                file, "NA", false, terminalPatchStatus
                ));
            moveFileResultToClassificationFolder(
                prNr, safeFileName, fileResultsDir, "NA"
                );
        } else {
            handleFileClassificationError(
                prNr, file, fileResultsDir, safeFileName
                );
            out.append(makePrFileResult(
                file, "ERROR", false, terminalPatchStatus
                ));
        }
        return;
    }

    int dupCount = 0;
    patchPath = savePatch(patchPath, patchName, patchLines, dupCount);

    const QString extension = jscpdExtensionFor(extInfo);

    if (gitChecks.addedFiles.contains(mainlineFileNameForRenamesOrAdditions)) {
        const bool jsonSaved = writeIndividualFileResultJson(
            resultsFilePath,
            file,
            prNr,
            sourceRepo,
            variantRepo,
            status,
            prRenamed,
            renamedFileName,
            destPath,
            QStringLiteral("resolved"),
            "NA",
            patchAvailable,
            patchStatus
            );
        if (!jsonSaved) {
            throw std::runtime_error(
                QString("Failed to save added-file results JSON: %1")
                    .arg(resultsFilePath).toStdString()
                );
        }
        markFileAsAddedNA(
            prNr,
            file,
            status,
            patchPath,
            fileResultsDir,
            safeFileName
            );
        out.append(makePrFileResult(file, "NA", patchAvailable, patchStatus));
        return;
    }

    // Only files with actual textual patch data need historical rename
    // resolution for similarity comparison.
    bool renamedDestPath = false;
    destPath = resolvePossibleRenamePath(
        destPath,
        divergentFileNameForRenamesOrAdditions,
        mainlineFileNameForRenamesOrAdditions,
        reverseRenameMap,
        renamedDestPath
        );
    const QString divergentPathType =
        renamedDestPath ? QStringLiteral("renamed")
                        : QStringLiteral("resolved");

    // A changed file normally always has at least one GitHub API entry, so
    // fileEntries.isEmpty() is not a reliable way to detect that the file is
    // absent from the divergent repository. Check the final resolved path
    // instead (including any rename resolution) before running similarity.
    const QFileInfo divergentFileInfo(destPath);
    if (!divergentFileInfo.exists() || !divergentFileInfo.isFile()) {
        const bool jsonSaved = writeIndividualFileResultJson(
            resultsFilePath,
            file,
            prNr,
            sourceRepo,
            variantRepo,
            status,
            prRenamed,
            renamedFileName,
            destPath,
            renamedDestPath ? "renamed" : "resolved",
            "NOT EXISTING",
            patchAvailable,
            patchStatus
            );
        if (!jsonSaved) {
            throw std::runtime_error(
                QString("Failed to save not-existing results JSON: %1")
                    .arg(resultsFilePath).toStdString()
                );
        }
        markFileAsNotExisting(
            prNr,
            file,
            fileResultsDir,
            safeFileName
            );
        out.append(makePrFileResult(file, "NE", patchAvailable, patchStatus));
        return;
    }

    const ClassificationWorkspace workspace = createWorkspace(prNr, safeFileName);

    copyFileForSimilarityCheck(destPath, patchPath, workspace);

    const QString localSrcFile =
        workspace.src + "/" + QFileInfo(patchPath).fileName();

    parsePatchFile(localSrcFile, workspace.src, extension);

    // The copied .patch file is only an intermediate input for hunk
    // generation. Removing it prevents broad exact-filename scans from also
    // analyzing the patch as the built-in diff format.
    QFile::remove(localSrcFile);

    const QStringList contextFiles = collectContextFiles(workspace, extInfo);

    const ClassificationResult classificationResult =
        runSimilarityClassification(workspace, extInfo, contextFiles);

    writeSingleFileResults(
        prNr,
        file,
        status,
        destPath,
        renamedDestPath,
        prRenamed,
        renamedFileName,
        patchAvailable,
        patchStatus,
        classificationResult,
        safeFileName,
        out
        );

    saveFinalFileResult(
        prNr,
        file,
        status,
        destPath,
        patchPath,
        classificationResult.classification,
        classificationResult.hunkClassifications
        );

    moveFileResultToClassificationFolder(
        prNr,
        safeFileName,
        fileResultsDir,
        classificationResult.classification
        );
}

QString GACPD::buildDivergentFilePath(
    const QString &newFileDir,
    const QString &fileName){
    return movisWorkingPath(
        "Results/Repos_files/" +
        rootFolder + "/" +
        variantRepo + "/" +
        newFileDir + "/" +
        fileName);
}

QString GACPD::buildPatchDirectoryPath(
    const QString &prNr,
    const QString &newFileDir){
    return movisWorkingPath(
        "Results/Repos_files/" +
        rootFolder + "/" +
        sourceRepo + "/" +
        prNr + "/patches/" +
        newFileDir);
}

QString GACPD::resolvePossibleRenamePath(
    const QString &currentDestPath,
    const QString &divergentFileName,
    const QString &mainlineFileName,
    const QMap<QString, QString> &reverseRenameMap,
    bool &renamedDestPath){
    QString destPath = normalizePath(currentDestPath);
    renamedDestPath = false;

    // Prefer a path that actually exists in the divergent snapshot before
    // walking historical rename maps. This is both more accurate and avoids
    // needless history traversal for rename-only PRs where the divergent fork
    // already uses the same path as mainline.
    if (QFileInfo(destPath).exists() && QFileInfo(destPath).isFile()) {
        return destPath;
    }

    // A GitHub PR rename is keyed new-name -> previous-name in
    // reverseRenameMap. First try the PR's previous filename directly in the
    // divergent snapshot. For a pure rename (GitHub changes == 0), this is the
    // most common and most reliable counterpart and requires no patch/hunk
    // processing at all.
    QString currentCheck = divergentFileName;
    if (reverseRenameMap.contains(mainlineFileName)) {
        const QString previousMainlineName =
            reverseRenameMap.value(mainlineFileName);
        currentCheck = previousMainlineName;

        const QString previousDestPath = movisWorkingPath(
            "Results/Repos_files/" +
            rootFolder + "/" +
            variantRepo + "/" +
            previousMainlineName
            );

        if (QFileInfo(previousDestPath).exists() &&
            QFileInfo(previousDestPath).isFile()) {
            renamedDestPath =
                normalizePath(previousMainlineName) !=
                normalizePath(mainlineFileName);
            return previousDestPath;
        }
    }

    for (const QStringList &cycle : cycles) {
        if (cycle.contains(currentCheck)) {
            const QString cyclePath = cycle.value(0);
            if (!cyclePath.isEmpty()) {
                destPath = movisWorkingPath(
                    "Results/Repos_files/" +
                    rootFolder + "/" +
                    variantRepo + "/" +
                    cyclePath);
                renamedDestPath =
                    normalizePath(cyclePath) != normalizePath(mainlineFileName);
                return normalizePath(destPath);
            }
        }
    }

    if (reverseRenameMap.contains(mainlineFileName)) {
        const QString previousMainlineName =
            reverseRenameMap.value(mainlineFileName);
        const QPair<QString, QString> renameResult =
            getRenamePath(
                previousMainlineName,
                renamesMainline,
                renames
                );

        QString currentDivergentName = renameResult.second;
        if (currentDivergentName.isEmpty()) {
            currentDivergentName = previousMainlineName;
        }

        destPath = movisWorkingPath(
            "Results/Repos_files/" +
            rootFolder + "/" +
            variantRepo + "/" +
            currentDivergentName);
        renamedDestPath =
            normalizePath(currentDivergentName) !=
            normalizePath(mainlineFileName);
        return normalizePath(destPath);
    }

    if (divergentFileName != mainlineFileName) {
        const QPair<QString, QString> renameResult =
            getRenamePath(
                divergentFileName,
                renamesMainline,
                renames
                );

        QString currentDivergentName = renameResult.second;
        if (currentDivergentName.isEmpty()) {
            currentDivergentName = divergentFileName;
        }

        destPath = movisWorkingPath(
            "Results/Repos_files/" +
            rootFolder + "/" +
            variantRepo + "/" +
            currentDivergentName);
        renamedDestPath =
            normalizePath(currentDivergentName) !=
            normalizePath(mainlineFileName);
    }

    return normalizePath(destPath);
}

void GACPD::copyFileForSimilarityCheck(
    const QString &destPath,
    const QString &patchPath,
    const ClassificationWorkspace &workspace){
    const QString localCmpFile =
        workspace.cmp + "/" + QFileInfo(destPath).fileName();

    const QString localSrcFile =
        workspace.src + "/" + QFileInfo(patchPath).fileName();

    QFile::remove(localCmpFile);
    QFile::remove(localSrcFile);

    if (!QFile::copy(destPath, localCmpFile)) {
        #ifdef QT_DEBUG
            qDebug() << "Failed to copy cmp file:"
                     << destPath << "->" << localCmpFile;
        #endif
    }

    if (!QFile::copy(patchPath, localSrcFile)) {
        #ifdef QT_DEBUG
            qDebug() << "Failed to copy src patch file:"
                     << patchPath << "->" << localSrcFile;
        #endif
    }
}

QString bundledNodeBinDir() {
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

static bool hasPinnedJscpd(const QString &root, const QString &executable) {
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

QString GACPD::jscpdBin(){
    #ifdef Q_OS_WIN
        const QString executable = QStringLiteral("jscpd.cmd");
    #else
        const QString executable = QStringLiteral("jscpd");
    #endif

    if (hasPinnedJscpd(movisLocalRoot(), executable)) {
        return QDir(movisLocalRoot()).filePath(
            "node_modules/.bin/" + executable);
    }

    #ifdef Q_OS_MACOS
        // Prefer the dependency bundled with this exact MOVis build.  A
        // writable Application Support copy may have been installed by an
        // older version of MOVis and must not silently override the pinned
        // JSCPD version shipped with the application.
        // Use the writable installation only as a fallback when the bundled
        // pinned dependency is genuinely unavailable.
        if (hasPinnedJscpd(movisDependencyRoot(), executable)) {
            return QDir(movisDependencyRoot()).filePath(
                "node_modules/.bin/" + executable);
        }
    #endif

    // Do not silently execute a different JSCPD version. Startup dependency
    // handling will run npm ci from the pinned lockfile; returning an empty
    // path here makes a failed/missing installation fail visibly instead of
    // changing research classifications.
    return QString();
}

std::optional<QJsonObject> GACPD::runJscpd(
    const ClassificationWorkspace &workspace,
    const FileExtensionInfo &extInfo,
    int tokenSize){
    const QString outputDir =
        QDir(workspace.root).absoluteFilePath("reports");

    QDir().mkpath(outputDir);

    const QString extension = jscpdExtensionFor(extInfo);
    const bool exactFileName = isExactFileNameMapping(extInfo);

    QStringList args;

    // Exact registered filenames need a broad scan so the original cmp file
    // (for example, cmp/Makefile or cmp/go.mod) is included together with the
    // generated hunk files. Ordinary extensions retain the narrower pattern.
    if (!exactFileName) {
        args << "--pattern" << QString("*.%1").arg(extension);
    }

    if (extInfo.registered) {
        const QString registeredFormat = registeredFormatFor(extInfo);

        if (registeredFormat.isEmpty()) {
            qWarning() << "Registered extension has no format mapping:"
                       << extInfo.extension;
            return std::nullopt;
        }

        // QProcess receives this as one argument. For .rst, the QMap entry
        // {".rst", {"rest"}} therefore produces exactly rest:rst.
        args << "--formats-exts"
             << QString("%1:%2").arg(registeredFormat, extension);

        if (exactFileName) {
            args << "--formats-names"
                 << QString("%1:%2").arg(registeredFormat,
                                          extInfo.extension);
        }
    }

    args << "--min-lines" << "1"
         << "--min-tokens" << QString::number(tokenSize)
         << "--reporters" << "json,html"
         << "--output" << outputDir
         << "--skip-local"
         << "src"
         << "cmp";

    QProcess process;
    process.setWorkingDirectory(workspace.root);

    #ifdef QT_DEBUG
        qDebug() << "Running JSCPD in:" << workspace.root;
        qDebug() << "Command:" << jscpdBin() << args;
    #endif

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

    const QString nodeBinDir = bundledNodeBinDir();
    const QString oldPath = env.value("PATH");

    #ifdef Q_OS_WIN
        env.insert("PATH", nodeBinDir + ";" + oldPath);
    #else
        env.insert("PATH", nodeBinDir + ":" + oldPath);
    #endif

    const QStringList reportPaths = {
        QDir(outputDir).filePath("jscpd-report.json"),
        QDir(outputDir).filePath("html/jscpd-report.json")
    };

    for (const QString &path : reportPaths) {
        QFile::remove(path);
    }

    process.setProcessEnvironment(env);
    process.start(jscpdBin(), args);

    if (!process.waitForFinished(-1)) {
        #ifdef QT_DEBUG
            qDebug() << "jscpd did not finish.";
            qDebug() << "stderr:" << process.readAllStandardError();
        #endif
        return std::nullopt;
    }

    const QByteArray stdoutText = process.readAllStandardOutput();
    const QByteArray stderrText = process.readAllStandardError();
    #ifdef QT_DEBUG

        if (!stdoutText.isEmpty()) {
            qDebug() << "jscpd stdout:" << stdoutText;
        }

        if (!stderrText.isEmpty()) {
            qDebug() << "jscpd stderr:" << stderrText;
        }
    #endif

    QString reportPath;

    for (const QString &path : reportPaths) {
        if (QFileInfo::exists(path)) {
            reportPath = path;
            break;
        }
    }

    if (reportPath.isEmpty()) {
        #ifdef QT_DEBUG
            qDebug() << "jscpd exit status/code:"
                     << process.exitStatus() << process.exitCode();
            qDebug() << "Could not find jscpd JSON report in:" << reportPaths;
        #endif
        return std::nullopt;
    }

    // A usable JSON report is the authoritative result. Some jscpd builds can
    // return a non-zero status for policy/threshold reasons even though clone
    // detection completed and the report was written successfully.
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        #ifdef QT_DEBUG
            qDebug() << "jscpd returned a non-zero status but produced a report:"
                     << process.exitStatus() << process.exitCode();
        #endif
    }

    QFile jsonFile(reportPath);

    if (!jsonFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        #ifdef QT_DEBUG
            qDebug() << "Could not open jscpd report:" << reportPath;
        #endif
        return std::nullopt;
    }

    QJsonParseError parseError;
    const QJsonDocument doc =
        QJsonDocument::fromJson(jsonFile.readAll(), &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        #ifdef QT_DEBUG
            qDebug() << "Failed to parse jscpd JSON:" << parseError.errorString();
        #endif
        return std::nullopt;
    }

    if (!doc.isObject()) {
        #ifdef QT_DEBUG
            qDebug() << "jscpd report was not a JSON object:" << reportPath;
        #endif
        return std::nullopt;
    }

    return doc.object();
}

QStringList GACPD::collectContextFiles(
    const ClassificationWorkspace &workspace,
    const FileExtensionInfo &extInfo){
    QStringList filesContext;

    const QList<int> tokensContext = {10};
    const QString contextReportPath =
        QDir(workspace.reports).filePath("jscpd-context-report.json");

    // The context detector must be physically isolated from the normal
    // additions/deletions checks. Running JSCPD over the whole src directory
    // lets normal hunk files participate in the 10-token context scan and can
    // make context ownership ambiguous. Build a temporary workspace containing
    // only hunk_N_context files plus the divergent cmp file.
    QTemporaryDir contextTempDir;
    if (!contextTempDir.isValid()) {
        qWarning() << "Could not create isolated JSCPD context workspace";
        QFile::remove(contextReportPath);
        return filesContext;
    }

    ClassificationWorkspace contextWorkspace;
    contextWorkspace.root = contextTempDir.path();
    contextWorkspace.src = QDir(contextWorkspace.root).filePath("src");
    contextWorkspace.cmp = QDir(contextWorkspace.root).filePath("cmp");
    contextWorkspace.patch = QDir(contextWorkspace.root).filePath("patch");
    contextWorkspace.reports = QDir(contextWorkspace.root).filePath("reports");

    QDir().mkpath(contextWorkspace.src);
    QDir().mkpath(contextWorkspace.cmp);
    QDir().mkpath(contextWorkspace.reports);

    static const QRegularExpression contextNamePattern(
        R"(^hunk_\d+_context(\.[^/\\]+)?$)",
        QRegularExpression::CaseInsensitiveOption
        );

    int copiedContextFiles = 0;
    const QDir normalSourceDir(workspace.src);
    const QStringList generatedFiles = normalSourceDir.entryList(
        QDir::Files | QDir::NoDotAndDotDot,
        QDir::Name
        );

    for (const QString &fileName : generatedFiles) {
        if (!contextNamePattern.match(fileName).hasMatch()) {
            continue;
        }

        const QString source = normalSourceDir.filePath(fileName);
        const QString target = QDir(contextWorkspace.src).filePath(fileName);
        if (QFile::copy(source, target)) {
            ++copiedContextFiles;
        }
    }

    const QDir normalCmpDir(workspace.cmp);
    const QStringList cmpFiles = normalCmpDir.entryList(
        QDir::Files | QDir::NoDotAndDotDot,
        QDir::Name
        );

    int copiedCmpFiles = 0;
    for (const QString &fileName : cmpFiles) {
        const QString source = normalCmpDir.filePath(fileName);
        const QString target = QDir(contextWorkspace.cmp).filePath(fileName);
        if (QFile::copy(source, target)) {
            ++copiedCmpFiles;
        }
    }

    QFile::remove(contextReportPath);

    if (copiedContextFiles == 0 || copiedCmpFiles == 0) {
        return filesContext;
    }

    for (int token : tokensContext) {
        std::optional<QJsonObject> maybeData = runJscpd(contextWorkspace, extInfo, token);

        if (!maybeData.has_value()) {
            continue;
        }

        // Preserve only the isolated context-vs-cmp report. The normal
        // additions/deletions files never participated in this JSCPD run.
        QFile contextReport(contextReportPath);
        if (contextReport.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            QByteArray contextJson = serializeOrderedJsonObject(
                maybeData.value(),
                0,
                {"statistics", "duplicates"}
                );
            contextJson += '\n';
            contextReport.write(contextJson);
            contextReport.close();
        } else {
            qWarning() << "Could not preserve JSCPD context report:"
                       << contextReportPath;
        }

        const QJsonObject sources =
            extractJscpdSources(maybeData.value(), extInfo, contextWorkspace);

        for (const QString &checkName : sources.keys()) {
            const QJsonObject checkObj = sources[checkName].toObject();
            const int duplicatedLines = checkObj["duplicatedLines"].toInt();
            const double percentage = checkObj["percentage"].toDouble();
            const bool hasClone = duplicatedLines > 0 || percentage > 0.0;

            if (checkName.contains("context") && hasClone) {
                const QStringList splitCheck = checkName.split("_");
                const QString contextHunk = splitCheck.mid(0, 2).join("_");
                if (!contextHunk.isEmpty() &&
                    !filesContext.contains(contextHunk)) {
                    filesContext.append(contextHunk);
                }
            }
        }
    }

    return filesContext;
}

static bool parseHunkCheckName(const QString &checkName,
                               int &hunkNumber,
                               QString &kind) {
    if (!checkName.startsWith("hunk_")) {
        return false;
    }

    const int numberEnd = checkName.indexOf('_', 5);
    if (numberEnd <= 5) {
        return false;
    }

    bool numberOk = false;
    const int parsedNumber =
        checkName.mid(5, numberEnd - 5).toInt(&numberOk);

    if (!numberOk || parsedNumber <= 0) {
        return false;
    }

    const QString suffix = checkName.mid(numberEnd + 1);

    if (suffix.startsWith("deletions")) {
        kind = "deletions";
    } else if (suffix.startsWith("additions")) {
        kind = "additions";
    } else {
        return false;
    }

    hunkNumber = parsedNumber;
    return true;
}

ClassificationResult GACPD::runSimilarityClassification(
    const ClassificationWorkspace &workspace,
    const FileExtensionInfo &extInfo,
    const QStringList &contextFiles){
    ClassificationResult result;

    QSet<int> allHunkNumbers;
    const QStringList generatedChecks = QDir(workspace.src).entryList(
        QDir::Files | QDir::NoDotAndDotDot,
        QDir::Name
        );

    // A context clone belongs to the hunk itself, not independently to its
    // additions/deletions projections. Both generated projections contain the
    // same unchanged hunk context, so duplicating that signal into separate
    // MO/ED context sets makes one shared context match look directional.
    QSet<int> hunkHasAdditions;
    QSet<int> hunkHasDeletions;
    QSet<int> hunksWithContextClone;

    for (const QString &checkName : generatedChecks) {
        int hunkNumber = 0;
        QString kind;

        if (!parseHunkCheckName(checkName, hunkNumber, kind)) {
            continue;
        }

        allHunkNumbers.insert(hunkNumber);

        if (kind == "deletions") {
            hunkHasDeletions.insert(hunkNumber);
        } else if (kind == "additions") {
            hunkHasAdditions.insert(hunkNumber);
        }
    }

    static const QRegularExpression contextHunkPattern(
        R"(^hunk_(\d+)$)",
        QRegularExpression::CaseInsensitiveOption
        );

    for (const QString &contextHunk : contextFiles) {
        const QRegularExpressionMatch match =
            contextHunkPattern.match(contextHunk.trimmed());
        if (!match.hasMatch()) {
            continue;
        }

        bool ok = false;
        const int hunkNumber = match.captured(1).toInt(&ok);
        if (ok && hunkNumber > 0) {
            hunksWithContextClone.insert(hunkNumber);
        }
    }

    QMap<int, QString> latestHunkClassifications;
    QMap<int, QString> latestHunkAdditionClassifications;
    QMap<int, QString> latestHunkDeletionClassifications;
    QSet<int> unresolvedHunks = allHunkNumbers;

    for (int hunkNumber : allHunkNumbers) {
        latestHunkClassifications[hunkNumber] = "NA";
        latestHunkAdditionClassifications[hunkNumber] =
            hunkHasAdditions.contains(hunkNumber) ? "NA" : "N/A";
        latestHunkDeletionClassifications[hunkNumber] =
            hunkHasDeletions.contains(hunkNumber) ? "NA" : "N/A";
    }

    QList<int> tokenSizes;

    if (scriptingLanguages.contains(extInfo.extension)) {
        tokenSizes = constant::scriptTokenSizes;
    } else {
        tokenSizes = constant::tokenSizes;
    }

    // Preserve the exact JSCPD duplicate records from the threshold at which
    // each hunk actually resolves. With per-hunk thresholding, the ordinary
    // jscpd-report.json is overwritten by later lower-threshold runs needed by
    // other hunks, so it is not a reliable visualization source by itself.
    QJsonArray selectedNormalDuplicates;

    const QString classificationReportPath =
        QDir(workspace.reports).filePath("jscpd-classification-report.json");
    QFile::remove(classificationReportPath);

    const QDir generatedSourceDir(workspace.src);
    const QStringList generatedSourceNames = generatedSourceDir.entryList(
        QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
    QSet<QString> generatedSourceNamesFolded;
    for (const QString &name : generatedSourceNames) {
        generatedSourceNamesFolded.insert(name.toCaseFolded());
    }

    // Threshold selection is per hunk, not per file. A hunk that finds normal
    // MO/ED evidence at 50 stops at 50; an unrelated unresolved hunk continues
    // independently to 40, 30, 20, and (for the configured scripting/config
    // extensions) 10. A result in one hunk must never stop lower-threshold
    // checks for another hunk.
    for (int jscpdToken : tokenSizes) {
        if (unresolvedHunks.isEmpty()) {
            break;
        }

        std::optional<QJsonObject> maybeData = runJscpd(workspace, extInfo, jscpdToken);

        if (!maybeData.has_value()) {
            continue;
        }

        QMap<int, int> hunkMoChecks;
        QMap<int, int> hunkEdChecks;

        const QJsonObject sources =
            extractJscpdSources(maybeData.value(), extInfo, workspace);

        // A normal additions/deletions file contains unchanged context lines
        // as well as the actual changed lines. A JSCPD match is normal MO/ED
        // evidence only when the duplicate range intersects a real '-' or '+'
        // line. Context-only overlap inside hunk_N_additions/deletions must not
        // count as a normal classification hit.
        const QHash<QString, bool> directClonePresence =
            directClonePresenceByCheck(maybeData.value(), workspace);

        for (const QString &checkName : sources.keys()) {
            int hunkNumber = 0;
            QString hunkKind;

            if (!parseHunkCheckName(checkName, hunkNumber, hunkKind) ||
                !unresolvedHunks.contains(hunkNumber)) {
                // This hunk already classified at a higher threshold. Do not
                // let a lower-threshold run alter its evidence, classification,
                // or similarity audit trail.
                continue;
            }

            const QJsonObject checkObj = sources[checkName].toObject();
            const double percentage = checkObj["percentage"].toDouble();
            const bool hasDirectClone =
                directClonePresence.value(checkName, false);

            if (hunkKind == "deletions") {
                hunkMoChecks[hunkNumber] = hasDirectClone ? 1 : 0;

                result.similarityLines.append(
                    QString("%1 (%2) - has a similarity of: %3% - direct change clone: %4\n")
                        .arg(checkName)
                        .arg(jscpdToken)
                        .arg(percentage)
                        .arg(hasDirectClone ? "yes" : "no")
                    );
            } else if (hunkKind == "additions") {
                hunkEdChecks[hunkNumber] = hasDirectClone ? 1 : 0;

                result.similarityLines.append(
                    QString("%1 (%2) - has a similarity of: %3% - direct change clone: %4\n")
                        .arg(checkName)
                        .arg(jscpdToken)
                        .arg(percentage)
                        .arg(hasDirectClone ? "yes" : "no")
                    );
            }
        }

        QSet<int> resolvedAtThisThreshold;

        for (int hunkNumber : unresolvedHunks) {
            const int hunkMo = hunkMoChecks.value(hunkNumber, 0);
            const int hunkEd = hunkEdChecks.value(hunkNumber, 0);

            // No normal classification for THIS hunk at THIS threshold. Leave
            // it unresolved so only this hunk falls through to the next token
            // size. Context is deliberately not consulted here.
            if (hunkMo == 0 && hunkEd == 0) {
                continue;
            }

            // Preserve which projection actually supplied the normal evidence.
            // Additions clone evidence is ED; deletions clone evidence is MO.
            // The opposite side remains NA when it exists but did not detect a
            // direct clone at the threshold where this hunk resolved.
            if (hunkEd > 0 && hunkHasAdditions.contains(hunkNumber)) {
                latestHunkAdditionClassifications[hunkNumber] = "ED";
            }
            if (hunkMo > 0 && hunkHasDeletions.contains(hunkNumber)) {
                latestHunkDeletionClassifications[hunkNumber] = "MO";
            }

            // Same established normal decision rule, but applied independently
            // to each hunk. A tie is MO. These are hit counts (0/1 per side),
            // never duplicated-token totals.
            if (hunkMo >= hunkEd) {
                latestHunkClassifications[hunkNumber] = "MO";
            } else {
                latestHunkClassifications[hunkNumber] = "ED";
            }

            resolvedAtThisThreshold.insert(hunkNumber);
        }

        // Keep only generated additions/deletions <-> cmp duplicates for hunks
        // that resolved at THIS threshold. This produces one merged report that
        // faithfully represents each hunk's selected normal classification
        // threshold instead of whichever global threshold happened to run last.
        const QJsonArray thresholdDuplicates =
            maybeData.value().value("duplicates").toArray();

        for (const QJsonValue &duplicateValue : thresholdDuplicates) {
            if (!duplicateValue.isObject()) {
                continue;
            }

            const QJsonObject duplicate = duplicateValue.toObject();
            const QString firstBase = jscpdReportedBaseName(
                duplicate.value("firstFile").toObject()
                    .value("name").toString());
            const QString secondBase = jscpdReportedBaseName(
                duplicate.value("secondFile").toObject()
                    .value("name").toString());

            const bool firstIsGenerated =
                generatedSourceNamesFolded.contains(firstBase.toCaseFolded());
            const bool secondIsGenerated =
                generatedSourceNamesFolded.contains(secondBase.toCaseFolded());

            if (firstIsGenerated == secondIsGenerated) {
                continue;
            }

            const QString generatedName =
                firstIsGenerated ? firstBase : secondBase;
            int duplicateHunkNumber = 0;
            QString duplicateHunkKind;
            if (!parseHunkCheckName(generatedName,
                                    duplicateHunkNumber,
                                    duplicateHunkKind) ||
                !resolvedAtThisThreshold.contains(duplicateHunkNumber)) {
                continue;
            }

            selectedNormalDuplicates.append(duplicate);
        }

        for (int hunkNumber : resolvedAtThisThreshold) {
            unresolvedHunks.remove(hunkNumber);
        }
    }

    // Write a visualization report even when it has zero duplicates. An empty
    // selected report correctly means that no hunk resolved through normal
    // direct evidence; falling back to the last raw threshold report could
    // otherwise resurrect context-only/additional weak matches.
    {
        QJsonObject selectedReport;
        selectedReport.insert("duplicates", selectedNormalDuplicates);
        selectedReport.insert("moviPerHunkThresholdSelection", true);

        QFile reportFile(classificationReportPath);
        if (reportFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            QByteArray reportJson = serializeOrderedJsonObject(
                selectedReport, 0,
                {"moviPerHunkThresholdSelection", "duplicates"});
            reportJson += '\n';
            reportFile.write(reportJson);
            reportFile.close();
        } else {
            qWarning() << "Could not preserve per-hunk JSCPD report:"
                       << classificationReportPath;
        }
    }

    // Side-specific diagnostic classifications should still expose matching
    // shared context even when the overall hunk already resolved normally on
    // the opposite side. This is transparency metadata only; it must never
    // change the selected overall hunk classification.
    //
    // Preserve the established side meanings:
    //   additions context -> MO
    //   deletions context -> ED
    //
    // Fill only unresolved side diagnostics here. Direct classifications found
    // during the normal threshold ladder keep priority and are not replaced.
    for (int hunkNumber : allHunkNumbers) {
        if (!hunksWithContextClone.contains(hunkNumber)) {
            continue;
        }

        if (hunkHasAdditions.contains(hunkNumber) &&
            latestHunkAdditionClassifications.value(hunkNumber) == "NA") {
            latestHunkAdditionClassifications[hunkNumber] = "MO";
        }

        if (hunkHasDeletions.contains(hunkNumber) &&
            latestHunkDeletionClassifications.value(hunkNumber) == "NA") {
            latestHunkDeletionClassifications[hunkNumber] = "ED";
        }
    }

    // Context is a per-hunk fallback and is consulted only after THIS hunk
    // failed every applicable normal threshold. The context itself is shared
    // by additions and deletions. When both projections exist, the same
    // context evidence applies to both sides, so preserve the established
    // tie rule (MO) instead of letting branch order incorrectly force ED.
    //
    // For one-sided hunks preserve the original fallback meaning:
    //   deletion-only + matching context -> ED
    //   addition-only + matching context -> MO
    //   additions + deletions + shared context -> MO (tie)
    for (int hunkNumber : unresolvedHunks) {
        if (!hunksWithContextClone.contains(hunkNumber)) {
            latestHunkClassifications[hunkNumber] = "NA";
            continue;
        }

        const bool hasDeletions = hunkHasDeletions.contains(hunkNumber);
        const bool hasAdditions = hunkHasAdditions.contains(hunkNumber);

        // Context is only classification evidence here because this hunk had
        // no normal result at any threshold. Keep the two side outcomes so the
        // visualizer can expose exactly how the shared fallback was interpreted.
        if (hasAdditions) {
            latestHunkAdditionClassifications[hunkNumber] = "MO";
        }
        if (hasDeletions) {
            latestHunkDeletionClassifications[hunkNumber] = "ED";
        }

        if (hasDeletions && !hasAdditions) {
            latestHunkClassifications[hunkNumber] = "ED";
        } else if (hasAdditions) {
            // Addition-only context keeps the original MO fallback. When both
            // sides exist, the shared context belongs equally to both and the
            // established MO-on-tie rule applies.
            latestHunkClassifications[hunkNumber] = "MO";
        } else {
            latestHunkClassifications[hunkNumber] = "NA";
        }
    }

    // File classification is derived ONLY from the final hunk
    // classifications. It is not independently recalculated from a file-wide
    // JSCPD pass. Preserve MO precedence: any MO hunk makes the file MO;
    // otherwise any ED hunk makes it ED; otherwise it is NA.
    int moTotal = 0;
    int edTotal = 0;

    for (QMap<int, QString>::const_iterator it = latestHunkClassifications.constBegin();
         it != latestHunkClassifications.constEnd(); ++it) {
        if (it.value() == "MO") {
            ++moTotal;
        } else if (it.value() == "ED") {
            ++edTotal;
        }
    }

    if (moTotal != 0) {
        result.classification = "MO";
    } else if (edTotal != 0) {
        result.classification = "ED";
    } else {
        result.classification = "NA";
    }

    const QList<int> sortedHunkNumbers = latestHunkClassifications.keys();
    for (int hunkNumber : sortedHunkNumbers) {
        HunkClassificationInfo hunk;
        hunk.hunkNumber = hunkNumber;
        hunk.classification = latestHunkClassifications.value(hunkNumber, "NA");
        hunk.additionsClassification =
            latestHunkAdditionClassifications.value(hunkNumber, "N/A");
        hunk.deletionsClassification =
            latestHunkDeletionClassifications.value(hunkNumber, "N/A");
        result.hunkClassifications.append(hunk);
    }

    return result;
}

QJsonObject GACPD::extractJscpdSources(
    const QJsonObject &data,
    const FileExtensionInfo &extInfo,
    const ClassificationWorkspace &workspace) const{
    // jscpd 4 exposed a per-file object at
    // statistics.formats.<format>.sources. Keep accepting that structure so
    // previously generated reports remain readable.
    const QJsonValue legacySources =
        data.value("statistics").toObject()
            .value("formats").toObject()
            .value(extInfo.language).toObject()
            .value("sources");

    if (legacySources.isObject()) {
        return legacySources.toObject();
    }

    // jscpd 5 changed "sources" to a numeric source count. Per-file clone
    // information now has to be reconstructed from the top-level duplicates
    // array. Initialize every generated hunk file first so a file with zero
    // matches is still visible to the ED/MO context fallback logic.
    const QDir sourceDir(workspace.src);
    const QStringList checkNames = sourceDir.entryList(
        QDir::Files | QDir::NoDotAndDotDot,
        QDir::Name
        );

    QHash<QString, QSet<int>> duplicatedLineNumbers;
    QHash<QString, QString> canonicalCheckNameByFoldedBase;

    for (const QString &checkName : checkNames) {
        duplicatedLineNumbers.insert(checkName, QSet<int>());
        canonicalCheckNameByFoldedBase.insert(checkName.toCaseFolded(), checkName);
    }

    const QJsonArray duplicates = data.value("duplicates").toArray();

    for (const QJsonValue &duplicateValue : duplicates) {
        if (!duplicateValue.isObject()) {
            continue;
        }

        const QJsonObject duplicate = duplicateValue.toObject();
        const QJsonObject firstFile = duplicate.value("firstFile").toObject();
        const QJsonObject secondFile = duplicate.value("secondFile").toObject();

        const QString firstBase =
            jscpdReportedBaseName(firstFile.value("name").toString());
        const QString secondBase =
            jscpdReportedBaseName(secondFile.value("name").toString());

        const QString firstName = canonicalCheckNameByFoldedBase.value(
            firstBase.toCaseFolded());
        const QString secondName = canonicalCheckNameByFoldedBase.value(
            secondBase.toCaseFolded());

        const bool firstIsCheck = !firstName.isEmpty();
        const bool secondIsCheck = !secondName.isEmpty();

        // Only cross-compare a generated hunk file with the divergent source
        // file. Ignore hunk-to-hunk and source-to-source duplicate records.
        if (firstIsCheck == secondIsCheck) {
            continue;
        }

        const QString checkName = firstIsCheck ? firstName : secondName;
        const QJsonObject checkFile = firstIsCheck ? firstFile : secondFile;

        const int startLine = checkFile.value("start").toInt();
        const int endLine = checkFile.value("end").toInt();

        if (startLine > 0 && endLine >= startLine) {
            QSet<int> &lineNumbers = duplicatedLineNumbers[checkName];

            for (int line = startLine; line <= endLine; ++line) {
                lineNumbers.insert(line);
            }
        }
    }

    QJsonObject sources;

    for (const QString &checkName : checkNames) {
        int totalLines = 0;
        QFile checkFile(sourceDir.filePath(checkName));

        if (checkFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            while (!checkFile.atEnd()) {
                checkFile.readLine();
                ++totalLines;
            }
        }

        const int duplicatedLines = duplicatedLineNumbers.value(checkName).size();
        const double percentage = totalLines > 0
            ? qMin(100.0, (100.0 * duplicatedLines) / totalLines)
            : 0.0;

        QJsonObject source;
        source.insert("duplicatedLines", duplicatedLines);
        source.insert("percentage", percentage);
        sources.insert(checkName, source);
    }

    return sources;
}

bool GACPD::startsWithAnyContext(
    const QString &checkName,
    const QStringList &contextFiles) const{
    for (const QString &context : contextFiles) {
        // Match a complete hunk prefix.  A plain startsWith("hunk_1") also
        // matches hunk_10, hunk_11, ... and leaks context evidence between
        // unrelated hunks.
        if (checkName.startsWith(context + "_")) {
            return true;
        }
    }

    return false;
}
