#ifndef GACPD_H
#define GACPD_H

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QFuture>
#include <QFutureSynchronizer>
#include <QJsonArray>
#include <QJsonObject>
#include <QMutex>
#include <QMutexLocker>
#include <QObject>
#include <QPair>
#include <QProcess>
#include <QStandardPaths>
#include <QStringList>
#include <QVariant>
#include <QVector>
#include <QtConcurrent>
#include <optional>

#include "Structures/AuditRows.h"
#include "Structures/ClassificationTypes.h"
#include "Structures/dataExtracting.h"

// One classification run: every pull request a variant repository merged from
// its source over a date range, classified file by file.
//
// A run always goes through the same six stages, and the implementation is
// split across one file per stage so a change lands in an obvious place:
//
//   1. find the pull requests in range      GACPD_Fetching.cpp
//   2. fetch their data from GitHub         GACPD_Fetching.cpp
//   3. patches into hunk files              GACPD_Patches.cpp
//   4. resolve renames from git history     GACPD_GitHistory.cpp
//   5. classify every changed file          GACPD_Classify.cpp
//   6. write the results out                GACPD_Results.cpp
//
// GACPD.cpp itself holds construction, the stage sequence in
// runClassification(), and the queries the stages share. The declarations
// below are grouped in the same order.
//
// A run is created on the calling thread and then moved to a worker thread;
// progress reaches the interface through the signals below, never by calling
// back into it.
class GACPD: public QObject{
    Q_OBJECT

    public:
        // `params` carries the run's settings positionally, in the order
        // MainWindow builds them:
        //
        //   0 result folder name        4 start of the date range (ISO)
        //   1 source repository         5 end of the date range (ISO)
        //   2 variant repository        6 capture the GitHub API responses
        //   3 GitHub tokens             7 fetch each PR's commit list
        explicit GACPD(const QVariantList& params, QObject* parent = nullptr);

        // Runs every stage in order. `prs` is only read for a single-PR run;
        // the other modes discover their own pull requests from the dates.
        void runClassification(const QStringList& prs,
                                const QString& dialogTitle,
                                bool parallel = true,
                                bool onePr = false);

        // Resolves the divergence and cut-off dates for a single-PR run.
        QString getDates(const QString& sourceRepo,
                          const QString& variantRepo,
                          const QStringList& tokenList,
                          int &ct,
                          const QString& cutOffDate = QString(),
                          const QString& divergenceDate = QString());

    signals:
        void classificationStarted(const QString& title);
        void stageChanged(const QString& stageMessage);
        void stageFinished(const QString& stageMessage);

        void classifyStarted(bool parallel);
        void prQueued(const QString& prNr, int totalFiles);
        void prStarted(const QString& prNr);
        void prFileProgress(const QString& prNr, int doneFiles, int totalFiles);
        void prFinished(const QString& prNr);
        void classificationFinished();

    private:
        // ---- The run itself (GACPD.cpp) ----------------------------------
        void setPrs(const QStringList& prs);
        bool prepareClassificationFolders();
        void classify(bool parallel);
        void processPr(const QString &prNr);

        // Bigger pull requests start first, so a parallel run does not finish
        // its short work and then sit waiting on one long tail.
        QStringList prsSortedByFileCount() const;
        int countFilesInPr(const QString& prNr) const;
        int countHunksInPatch(const QString& patch) const;
        int countHunksInPr(const QString& prNr) const;

        bool isInterestingClass(const QString &patchClass);
        QString safeName(QString file);
        QString normalizePath(QString path);
        void removeAllFiles(const QString& directoryPath);
        void copyDirAndRemoveOriginal(const QString &srcPath, const QString &dstPath);

        // ---- Stages 1-2: pull requests and their data (GACPD_Fetching.cpp)
        QStringList extractPatches(const QString& choosenDivergenceDate,
                                   const QString& choosenCutOffDate);

        void fetchPrData();

        prDataExtractor fetchPullRequestData(const QString &mainline,
                                             const QString &variant,
                                             const QStringList &pullrequests,
                                             const QString &variantSha,
                                             const QStringList &tokenList,
                                             int& ct);

        VariantShaExtractor getVariantSha(const QString &variant,
                                          const QString &cutOffDate,
                                          const QStringList &tokenList,
                                          int& ct);

        gitFiles getGitFiles(const QString& mainline,
                             const QString& baseSha,
                             const QString& prnum);

        // ---- Stage 3: patches into hunk files (GACPD_Patches.cpp) --------
        void createDataFrame();

        void parsePatchFile(const QString& patchFile,
                            const QString& outputDir,
                            const QString& extension);

        void saveHunkFiles(int hunkId,
                            const QString &outputDir,
                            const QStringList &additionsHunks,
                            int counts,
                            const QString &typeOfChange);

        // ---- Stage 4: renames from git history (GACPD_GitHistory.cpp) ----
        void ensureGitFoldersExists();
        void createGitFolder(const QString &folder, const QString &repoName);
        QString findReusableClone(const QString &currentRoot,
                                  const QString &owner,
                                  const QString &repo) const;
        void removeGitFolder(const QString &folder);

        void obtainGitInformation(const QStringList &tokenList, int &ct);
        void getGitInformation(const QStringList &tokenList, int &ct);

        QMap<QString, QString> obtainGitRenameHistory(const QString &startDate,
                                                      const QString &endDate,
                                                      const QString &repoPath);

        QString traceBackToOrigin(QString file,
                                  const QMap<QString, QString> &renames);
        QString traceForwardToLatest(QString file,
                                     const QMap<QString, QString> &renames);
        QPair<QString, QString> getRenamePath(const QString &filename,
                                              const QMap<QString, QString> &mainline,
                                              const QMap<QString, QString> &divergent);

        // Two files that swap names form a rename cycle, which has to be
        // detected rather than followed forever.
        CycleSearchResult findCycleFromFile(const QString &start,
                                            const QMap<QString, QString> &renames,
                                            const QSet<QString> &visited);
        QVector<QStringList> findRenameCycles(const QMap<QString, QString> &renames);

        // ---- Stage 5: classifying files (GACPD_Classify.cpp) -------------
        void processPrCommitFiles(
            const QString &prNr,
            const gitFiles &gitChecks,
            const QMap<QString, QString> &reverseRenameMap,
            QJsonArray &out);

        void processSingleFile(
            const QString &prNr,
            const QString &file,
            const QVariantList &fileEntries,
            const gitFiles &gitChecks,
            const QMap<QString, QString> &reverseRenameMap,
            QJsonArray &out);

        void classifyExistingFile(
            const QString &prNr,
            const QString &file,
            const QVariantList &fileEntries,
            const gitFiles &gitChecks,
            const QMap<QString, QString> &reverseRenameMap,
            const QString &fileResultsDir,
            const QString &safeFileName,
            const FileExtensionInfo &extInfo,
            QJsonArray &out);

        FileExtensionInfo detectFileExtension(const QString &file) const;

        // Where the file, its counterpart and the reports live while jscpd
        // runs over them.
        ClassificationWorkspace createWorkspace(
            const QString &prNr,
            const QString &safeFileName);

        QString buildDivergentFilePath(
            const QString &newFileDir,
            const QString &fileName);

        QString buildPatchDirectoryPath(
            const QString &prNr,
            const QString &newFileDir);

        QString resolvePossibleRenamePath(
            const QString &currentDestPath,
            const QString &divergentFileName,
            const QString &mainlineFileName,
            const QMap<QString, QString> &reverseRenameMap,
            bool &renamedDestPath);

        void copyFileForSimilarityCheck(
            const QString &destPath,
            const QString &patchPath,
            const ClassificationWorkspace &workspace);

        // Clone detection. runSimilarityClassification() walks descending
        // token thresholds until the evidence settles.
        QString jscpdBin();

        std::optional<QJsonObject> runJscpd(
            const ClassificationWorkspace &workspace,
            const FileExtensionInfo &extInfo,
            int tokenSize);

        QStringList collectContextFiles(
            const ClassificationWorkspace &workspace,
            const FileExtensionInfo &extInfo);

        ClassificationResult runSimilarityClassification(
            const ClassificationWorkspace &workspace,
            const FileExtensionInfo &extInfo,
            const QStringList &contextFiles);

        QJsonObject extractJscpdSources(
            const QJsonObject &data,
            const FileExtensionInfo &extInfo,
            const ClassificationWorkspace &workspace) const;

        bool startsWithAnyContext(
            const QString &checkName,
            const QStringList &contextFiles) const;

        // ---- Stage 6: writing results out (GACPD_Results.cpp) ------------
        void initializeFileResult(const QString &prNr, const QString &file);

        void saveFinalFileResult(
            const QString &prNr,
            const QString &file,
            const QString &status,
            const QString &destPath,
            const QString &patchPath,
            const QString &classification,
            const QVector<HunkClassificationInfo> &hunkClassifications);

        void writeSingleFileResults(
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
            QJsonArray &prOut);

        void moveFileResultToClassificationFolder(
            const QString &prNr,
            const QString &safeFileName,
            const QString &fileResultsDir,
            const QString &classification);

        // The four outcomes that skip clone detection entirely.
        void markFileAsOtherExtension(
            const QString &prNr,
            const QString &file,
            const QString &fileResultsDir,
            const QString &safeFileName);

        void markFileAsNotExisting(
            const QString &prNr,
            const QString &file,
            const QString &fileResultsDir,
            const QString &safeFileName);

        void markFileAsAddedNA(
            const QString &prNr,
            const QString &file,
            const QString &status,
            const QString &patchPath,
            const QString &fileResultsDir,
            const QString &safeFileName);

        void handleFileClassificationError(
            const QString &prNr,
            const QString &file,
            const QString &fileResultsDir,
            const QString &safeFileName,
            const QString &errorReason = QString());

        void writePrHeader(QJsonObject &out, const QString &prNr);
        void writeGitChecks(QJsonObject &out, const gitFiles &gitChecks);

        QMap<QString, QString> buildReverseRenameMap(
            const QMap<QString, QString> &renamedFiles);

        QMap<QString, QVariantMap> finalClass(const QMap<QString, QVariantMap>& resultDict);
        void writeClassificationAudits();
        void finalizeResults();

        // ---- Run settings, taken from the params list --------------------
        QString rootFolder;
        QString sourceRepo;
        QString variantRepo;
        QStringList tokenList;
        QString startOffDate;
        QString cutOffDate;

        // Optional expensive GitHub diagnostics. Both default to off so the
        // normal classification path stays fast.
        bool generateApiJsonFile = false;
        bool fetchPrCommitList = false;
        // Keep the repository working copies after the run and seed new
        // clones from existing local ones (dataset speed-up).
        bool skipRepoDeletion = false;

        // ---- Run state ----------------------------------------------------
        QStringList pullRequests;
        QMap<QString, QVariantMap> repoData;      // fetched pull request data
        QMap<QString, QVariantMap> repoResults;   // per-file outcomes
        QMap<QString, QVariantMap> prClassifications;

        // Rename maps for each side, plus the cycles found in them.
        QMap<QString, QString> renames;
        QMap<QString, QString> renamesMainline;
        QVector<QStringList> cycles;

        // Rows for the CSV audits written at the end of the run.
        QVector<PatchRow> dfPatches;
        QVector<FileClassRow> dfFilesClasses;
        QVector<PatchClassRow> dfPatchClasses;

        // Index of the GitHub token currently in use.
        int ct = 0;

        QString divergenceDate;
        QString forkDate;
        int aheadBy;
        int behindBy;
        QString modernDay;
        QList<double> timers;

        QString savedFiles = "Results/Repos_files/";
        QString repoClones = "Results/Repos_clones/";

        // A parallel run classifies several pull requests at once, so writes
        // into repoResults have to be serialized.
        QMutex repoResultsMutex;
};

#endif // GACPD_H
