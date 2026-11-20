// GACPD: one classification run from start to finish.
//
// A run goes through the same stages whichever mode started it:
//
//   1. find the pull requests in range          GACPD_Fetching.cpp
//   2. fetch their data from GitHub             GACPD_Fetching.cpp
//   3. turn their patches into hunk files       GACPD_Patches.cpp
//   4. resolve renames from the git history     GACPD_GitHistory.cpp
//   5. classify every changed file              GACPD_Classify.cpp
//   6. write the results out                    GACPD_Results.cpp
//
// This file holds the run itself: construction, the stage sequence, and the
// small queries the stages share.

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

GACPD::GACPD(const QVariantList& params, QObject* parent) : QObject(parent){
    rootFolder = params[0].toString();
    sourceRepo = params[1].toString();
    variantRepo = params[2].toString();
    tokenList = params[3].toStringList();
    startOffDate = params[4].toString();
    cutOffDate = params[5].toString();
    generateApiJsonFile = params.size() > 6 ? params[6].toBool() : false;
    fetchPrCommitList = params.size() > 7 ? params[7].toBool() : false;
    skipRepoDeletion = params.size() > 8 ? params[8].toBool() : false;

    repoResults.clear();
    repoData.clear();
    prClassifications.clear();

    renames.clear();
    renamesMainline.clear();
    cycles.clear();
    pullRequests.clear();

    divergenceDate.clear();
    forkDate.clear();
    aheadBy = 0;
    behindBy = 0;
    modernDay.clear();
    ct = 0;
    dfPatches.clear();
    dfFilesClasses.clear();
    dfPatchClasses.clear();
}

int GACPD::countFilesInPr(const QString& prNr) const {
    int count = 0;

    const QVariantMap prMap = repoData.value(prNr);
    const QVariantList commitsData = prMap.value("commits_data").toList();

    QSet<QString> uniqueFiles;

    for (const QVariant& filesVariant : commitsData) {
        const QVariantMap files = filesVariant.toMap();

        for (QVariantMap::const_iterator it = files.constBegin(); it != files.constEnd(); it++) {
            uniqueFiles.insert(it.key());
        }
    }

    count = uniqueFiles.size();
    return count;
}

int GACPD::countHunksInPatch(const QString& patch) const {
    int count = 0;
    const QStringList lines = patch.split('\n');

    for (const QString &line : lines) {
        if (line.startsWith("@@")) {
            ++count;
        }
    }

    return count;
}

int GACPD::countHunksInPr(const QString& prNr) const {
    int count = 0;
    const QVariantMap prMap = repoData.value(prNr);
    const QVariantList commitsData = prMap.value("commits_data").toList();

    for (const QVariant &filesVariant : commitsData) {
        const QVariantMap files = filesVariant.toMap();

        for (QVariantMap::const_iterator it = files.constBegin(); it != files.constEnd(); ++it) {
            const QVariantList fileEntries = it.value().toList();

            if (fileEntries.isEmpty()) {
                continue;
            }

            const QString patch = fileEntries.first().toMap().value("patch").toString();
            count += countHunksInPatch(patch);
        }
    }

    return count;
}

QStringList GACPD::prsSortedByFileCount() const {
    QStringList prs = repoData.keys();

    std::sort(prs.begin(), prs.end(),
              [this](const QString& a, const QString& b) {
                  return countFilesInPr(a) > countFilesInPr(b);
              });

    return prs;
}

void GACPD::setPrs(const QStringList& prs){
    for(const QString& pr: prs){
        pullRequests.append(pr);
    }
}

void GACPD::removeAllFiles(const QString& directoryPath){
    QDir dir(directoryPath);

    // Check if the directory exists
    if (!dir.exists()) {
        qDebug() << "The specified directory does not exist or is not a directory.";
        return;
    }

    // Get all files and directories inside the directory
    QFileInfoList entries = dir.entryInfoList(
        QDir::NoDotAndDotDot | QDir::AllEntries);

    for (const QFileInfo& entry : entries) {
        const QString filePath = entry.absoluteFilePath();

        bool success = false;

        // Remove file or symbolic link
        if (entry.isFile() || entry.isSymLink()) {
            success = QFile::remove(filePath);
        }// Recursively remove directory
        else if (entry.isDir()) {
            QDir subDir(filePath);
            success = subDir.removeRecursively();
        }

        if (!success) {
            qDebug() << "Failed to delete" << filePath;
        }
    }
}

bool GACPD::isInterestingClass(const QString &patchClass){
    const QStringList interestingClasses = {"ED", "MO", "SP"};
    return interestingClasses.contains(patchClass);
}

QString GACPD::safeName(QString file){
    // Flattening the complete path by replacing both '/' and '.' with '_'
    // is not collision-safe (for example a/b_c.py and a_b/c.py collapse to
    // the same directory name). Use a short hash of the complete normalized
    // path for uniqueness and retain the basename at the end so existing
    // extension-oriented result inspection remains readable.
    file = normalizePath(file);

    const QByteArray digest = QCryptographicHash::hash(
        file.toUtf8(),
        QCryptographicHash::Sha1
        ).toHex().left(12);

    QString baseName = QFileInfo(file).fileName();
    if (baseName.isEmpty()) {
        baseName = file;
    }

    baseName.replace("/", "_");
    baseName.replace("\\", "_");
    baseName.replace(".", "_");

    return QString::fromLatin1(digest) + "_" + baseName;
}

QString GACPD::normalizePath(QString path){
    path = QDir::cleanPath(path);
    path.replace("\\", "/");
    return path;
}

void GACPD::copyDirAndRemoveOriginal(const QString &srcPath, const QString &dstPath) {
    QDir dstDir;
    dstDir.mkpath(QFileInfo(dstPath).absolutePath());

    QDir srcDir(srcPath);
    if (srcDir.exists()) {
        srcDir.rename(srcPath, dstPath);
    }
}

bool GACPD::prepareClassificationFolders(){
    const QString resultsPath = movisResultsPath(rootFolder);

    QString writeError;
    if (!movisTestWritable(resultsPath, &writeError)) {
        qWarning() << "Failed to create/write results directory:"
                   << resultsPath << writeError;
        return false;
    }

    for (const QString &prNr : repoData.keys()) {
        const QString prDir =
            movisResultsPath(rootFolder + "/" + prNr);

        QDir dir(prDir);

        if (dir.exists() && !dir.removeRecursively()) {
            qWarning() << "Failed to remove existing PR result directory:" << prDir;
            return false;
        }

        if (!QDir().mkpath(prDir)) {
            qWarning() << "Failed to create PR result directory:" << prDir;
            return false;
        }

        QString prWriteError;
        if (!movisTestWritable(prDir, &prWriteError)) {
            qWarning() << "PR result directory is not writable:"
                       << prDir << prWriteError;
            return false;
        }
    }

    // Do NOT create global src/cmp/reports folders anymore.
    // Each file gets its own local workspace instead.
    return true;
}

void GACPD::runClassification(const QStringList& prs,
                              const QString& dialogTitle,
                              bool parallel,
                              bool onePr) {

    emit classificationStarted(dialogTitle);
    emit stageChanged("Finding all acceptable pull requests...");

    QStringList prsLocal;
    try {
        if (!onePr) {
            prsLocal = extractPatches(startOffDate, cutOffDate);
        } else {
            prsLocal = prs;
            getDates(sourceRepo, variantRepo, tokenList, ct);
        }
    } catch (const std::exception &error) {
        emit stageFinished(QString("Run incomplete: PR discovery failed: %1").arg(QString::fromUtf8(error.what())));
        emit classificationFinished();
        return;
    }
    setPrs(prsLocal);
    emit stageFinished("Finished getting pull requests.");

    if(!prsLocal.isEmpty()){
        emit stageChanged("Fetching pull request data...");
        try {
            fetchPrData();
        } catch (const std::exception &error) {
            emit stageFinished(QString("Run incomplete: %1").arg(QString::fromUtf8(error.what())));
            emit classificationFinished();
            return;
        }
        QStringList missingPrs;
        for (const QString &pr : prsLocal) if (!repoData.contains(pr)) missingPrs.append(pr);
        if (!missingPrs.isEmpty()) {
            emit stageFinished(QString("Run incomplete: %1 selected PRs failed to fetch. See github_fetch_audit.csv.")
                               .arg(missingPrs.size()));
            emit classificationFinished();
            return;
        }
        emit stageFinished("Finished fetching pull request data.");

        emit stageChanged("Ensuring Git folders exist...");
        ensureGitFoldersExists();
        emit stageFinished("Git folders are ready.");

        emit stageChanged("Obtaining Git rename and repository information...");
        try {
            obtainGitInformation(tokenList, ct);
        } catch (const std::exception &error) {
            emit stageFinished(QString("Run incomplete: %1").arg(QString::fromUtf8(error.what())));
            emit classificationFinished();
            return;
        }
        emit stageFinished("Git information obtained.");
    }
    #ifdef QT_DEBUG
        qDebug()<<"============================================";
    #endif

    emit stageChanged("Preparing result folders...");
    if (!prepareClassificationFolders()) {
        const QString failedPath = movisResultsPath(rootFolder);
        qWarning() << "Unable to create writable result folders at:" << failedPath;
        emit stageFinished(
            QString("Cannot write result folders at: %1").arg(failedPath));
        emit classificationFinished();
        return;
    }
    emit stageFinished("Result folders are ready.");

    emit stageChanged("Classifying pull requests...");
    classify(parallel);
    if (prClassifications.size() != repoData.size()) {
        emit stageFinished("Run incomplete: selected and classified PR counts differ. Inspect classification_audit.csv.");
        emit classificationFinished();
        return;
    }
    emit stageFinished("Classification completed.");

    emit stageChanged("Creating data frames...");
    createDataFrame();
    emit stageFinished("Data frames created.");

    #ifdef QT_DEBUG
        qDebug()<<"============================================";
    #endif

    emit stageChanged("Finalizing results...");
    finalizeResults();
    emit stageFinished("Results finalized.");

    // Repos_files and Repos_clones are temporary working copies only.  All
    // persistent classification artifacts have already been written to
    // Repos_results by this point, so clear both repository caches after every
    // run to prevent large datasets from accumulating on disk.  Use the
    // writable working root on macOS and the existing application root on
    // Windows/Linux through movisWorkingPath().  Keep the two top-level
    // directories themselves so the next run can start without recreating the
    // runtime layout.
    if (skipRepoDeletion) {
        emit stageChanged("Keeping temporary repository data (skip repository deletion)...");
    } else {
        emit stageChanged("Cleaning temporary repository data...");
        removeAllFiles(movisWorkingPath("Results/Repos_files"));
        removeAllFiles(movisWorkingPath("Results/Repos_clones"));
    }

    // Be defensive in case either directory was removed externally while the
    // run was active.  Empty directories consume effectively no space and are
    // expected by the normal runtime layout.
    QDir().mkpath(movisWorkingPath("Results/Repos_files"));
    QDir().mkpath(movisWorkingPath("Results/Repos_clones"));
    emit stageFinished(skipRepoDeletion
                           ? "Temporary repository data kept."
                           : "Temporary repository data removed.");

    #ifdef QT_DEBUG
        qDebug()<<"Final Time output (inseconds): ";
        for(const double& time : timers){
            qDebug()<<time<<" seconds";
        }
    #endif
    emit classificationFinished();
}

void GACPD::classify(bool parallel) {
    #ifdef QT_DEBUG
        qDebug() << QString("Starting Classification for %1 - %2")
                    .arg(sourceRepo, variantRepo);
    #endif

    QElapsedTimer timer;
    timer.start();

    emit classifyStarted(parallel);

    const QStringList sortedPrs = prsSortedByFileCount();

    for (const QString& prNr : sortedPrs) {
        emit prQueued(prNr, countFilesInPr(prNr));
    }

    if (parallel) {
        // Runs on the global thread pool. Pass a QThreadPool with a smaller
        // maxThreadCount to QtConcurrent::run if this needs throttling.
        QFutureSynchronizer<void> synchronizer;

        for (const QString &prNr : repoData.keys()) {
            synchronizer.addFuture(QtConcurrent::run([this, prNr]() {
                processPr(prNr);
            }));
        }

        synchronizer.waitForFinished();
    } else {
        for (const QString& prNr : sortedPrs) {
            processPr(prNr);
        }
    }

    prClassifications = finalClass(repoResults);
    writeClassificationAudits();

    const double seconds = timer.elapsed() / 1000.0;
    #ifdef QT_DEBUG
        QTextStream(stdout) << "---" << seconds << " seconds ---" << Qt::endl;
    #endif
    timers.append(seconds);
}

void GACPD::processPr(const QString& prNr) {
    emit prStarted(prNr);

    const QString filePath =
        movisResultsPath(rootFolder + "/" + prNr + "/pr_results.json");

    QJsonObject out;

    writePrHeader(out, prNr);

    // Build added/renamed metadata from the same fully paginated GitHub PR
    // file response that is actually being classified. This avoids silently
    // writing empty addedFilesSkipped/renamedFiles arrays when a separate
    // local git fetch/diff fails, and keeps the saved metadata tied to the
    // exact fetched dataset.
    const gitFiles gitChecks =
        gitFilesFromFetchedPrData(repoData.value(prNr));

    writeGitChecks(out, gitChecks);

    const QMap<QString, QString> reverseRenameMap =
        buildReverseRenameMap(gitChecks.renamedFiles);

    QJsonArray files;
    processPrCommitFiles(prNr, gitChecks, reverseRenameMap, files);
    out["files"] = files;

    out["recommendations"] = "TBO";

    const QStringList prFieldOrder = {
        "classifiedPR",
        "prTitle",
        "prDescription",
        "prLocation",
        "repoDivergenceDate",
        "cutoffDate",
        "addedFilesSkipped",
        "renamedFiles",
        "files",
        "recommendations"
    };

    const bool prJsonSaved = writeOrderedJsonObjectFile(
        filePath,
        out,
        "PR JSON result",
        prFieldOrder
        );

    if (!prJsonSaved) {
        qCritical() << "PR classification finished but required pr_results.json"
                    << "could not be saved/verified for PR" << prNr
                    << ":" << filePath;
    }

    emit prFinished(prNr);
}
