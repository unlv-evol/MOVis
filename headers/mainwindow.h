#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <QDateTime>
#include <QObject>
#include <QStringList>
#include <QVariantList>

#include "ui/ClassificationProgressModel.h"

// Backing object for Main.qml.
//
// MOVis has three ways to start a classification run, and they share the same
// project name and GitHub tokens:
//
//   Normal   one source/variant pair over a date range      submitNormal()
//   One PR   a single pull request, by number               submitOnePullRequest()
//   Dataset  many pairs in one batch                        submitDataset()
//
// Everything below is grouped in that order: the shared state first, then one
// section per run mode, then the supporting features (dependencies, results
// viewer, token storage, progress reporting).
//
// Every property here is read and written by QML directly, so these names are
// the names used in the .qml files. Renaming one means renaming it there too.
class MainWindow final : public QObject
{
    Q_OBJECT

    // ---- Shared by every run mode --------------------------------------------
    Q_PROPERTY(QString projectName READ projectName
               WRITE setProjectName NOTIFY stateChanged)
    Q_PROPERTY(QString tokens READ tokens WRITE setTokens NOTIFY stateChanged)
    Q_PROPERTY(QString resultsRoot READ resultsRoot CONSTANT)

    // ---- Normal run ----------------------------------------------------------
    Q_PROPERTY(QString sourceRepository READ sourceRepository
               WRITE setSourceRepository NOTIFY stateChanged)
    Q_PROPERTY(QString variantRepository READ variantRepository
               WRITE setVariantRepository NOTIFY stateChanged)
    Q_PROPERTY(QString divergenceDate READ divergenceDate
               WRITE setDivergenceDate NOTIFY stateChanged)
    Q_PROPERTY(QString cutoffDate READ cutoffDate
               WRITE setCutoffDate NOTIFY stateChanged)
    Q_PROPERTY(bool generateApiJsonNormal READ generateApiJsonNormal
               WRITE setGenerateApiJsonNormal NOTIFY stateChanged)
    Q_PROPERTY(bool fetchPrCommitListNormal READ fetchPrCommitListNormal
               WRITE setFetchPrCommitListNormal NOTIFY stateChanged)
    // The date range and its Submit button only appear once a divergence date
    // has been resolved.
    Q_PROPERTY(bool dateRangeVisible READ dateRangeVisible NOTIFY stateChanged)
    Q_PROPERTY(bool normalSubmitVisible READ normalSubmitVisible NOTIFY stateChanged)

    // ---- Single pull request -------------------------------------------------
    Q_PROPERTY(QString onePrSourceRepository READ onePrSourceRepository
               WRITE setOnePrSourceRepository NOTIFY stateChanged)
    Q_PROPERTY(QString onePrVariantRepository READ onePrVariantRepository
               WRITE setOnePrVariantRepository NOTIFY stateChanged)
    Q_PROPERTY(QString pullRequestNumber READ pullRequestNumber
               WRITE setPullRequestNumber NOTIFY stateChanged)
    Q_PROPERTY(bool generateApiJsonOnePr READ generateApiJsonOnePr
               WRITE setGenerateApiJsonOnePr NOTIFY stateChanged)
    Q_PROPERTY(bool fetchPrCommitListOnePr READ fetchPrCommitListOnePr
               WRITE setFetchPrCommitListOnePr NOTIFY stateChanged)

    // ---- Dataset batch -------------------------------------------------------
    Q_PROPERTY(QString datasetSources READ datasetSources
               WRITE setDatasetSources NOTIFY stateChanged)
    Q_PROPERTY(QString datasetVariants READ datasetVariants
               WRITE setDatasetVariants NOTIFY stateChanged)
    Q_PROPERTY(bool generateApiJsonDataset READ generateApiJsonDataset
               WRITE setGenerateApiJsonDataset NOTIFY stateChanged)
    Q_PROPERTY(bool fetchPrCommitListDataset READ fetchPrCommitListDataset
               WRITE setFetchPrCommitListDataset NOTIFY stateChanged)
    Q_PROPERTY(bool runDatasetReversed READ runDatasetReversed
               WRITE setRunDatasetReversed NOTIFY stateChanged)
    Q_PROPERTY(bool skipRepoDeletionDataset READ skipRepoDeletionDataset
               WRITE setSkipRepoDeletionDataset NOTIFY stateChanged)
    Q_PROPERTY(bool datasetEndDateLimited READ datasetEndDateLimited
               WRITE setDatasetEndDateLimited NOTIFY stateChanged)
    Q_PROPERTY(QString datasetEndDateLimit READ datasetEndDateLimit
               WRITE setDatasetEndDateLimit NOTIFY stateChanged)

    // ---- Dependency installation modal ---------------------------------------
    Q_PROPERTY(bool dependencyInstallInProgress READ dependencyInstallInProgress
               NOTIFY dependencyProgressChanged)
    Q_PROPERTY(QString dependencyProgressTitle READ dependencyProgressTitle
               NOTIFY dependencyProgressChanged)
    Q_PROPERTY(QString dependencyProgressMessage READ dependencyProgressMessage
               NOTIFY dependencyProgressChanged)

    // ---- Classification progress modal ---------------------------------------
    Q_PROPERTY(bool classificationProgressVisible READ classificationProgressVisible
               NOTIFY classificationProgressChanged)
    Q_PROPERTY(QString classificationProgressTitle READ classificationProgressTitle
               NOTIFY classificationProgressChanged)
    Q_PROPERTY(QString classificationStage READ classificationStage
               NOTIFY classificationProgressChanged)
    Q_PROPERTY(QAbstractListModel *classificationProgress READ classificationProgress
               CONSTANT)

    // ---- Results viewer ------------------------------------------------------
    Q_PROPERTY(bool advancedResultsVisible READ advancedResultsVisible
               NOTIFY stateChanged)

public:
    explicit MainWindow(QObject *parent = nullptr);

    // ---- Shared by every run mode --------------------------------------------
    QString projectName() const { return m_projectName; }
    QString tokens() const { return m_tokens; }
    QString resultsRoot() const;

    void setProjectName(const QString &value);
    void setTokens(const QString &value);

    Q_INVOKABLE void saveSettings();

    // ---- Normal run ----------------------------------------------------------
    QString sourceRepository() const { return m_sourceRepository; }
    QString variantRepository() const { return m_variantRepository; }
    QString divergenceDate() const;
    QString cutoffDate() const;
    bool generateApiJsonNormal() const { return m_generateApiJsonNormal; }
    bool fetchPrCommitListNormal() const { return m_fetchPrCommitListNormal; }
    bool dateRangeVisible() const { return m_dateRangeVisible; }
    bool normalSubmitVisible() const { return m_normalSubmitVisible; }

    void setSourceRepository(const QString &value);
    void setVariantRepository(const QString &value);
    void setDivergenceDate(const QString &value);
    void setCutoffDate(const QString &value);
    void setGenerateApiJsonNormal(bool value);
    void setFetchPrCommitListNormal(bool value);

    Q_INVOKABLE void checkDivergenceDate();
    // Detached-pair override: skips the fork-network compare and fills the
    // date range from repository metadata so it can be adjusted by hand.
    Q_INVOKABLE void overrideDivergenceDate();
    Q_INVOKABLE void submitNormal();

    // ---- Single pull request -------------------------------------------------
    QString onePrSourceRepository() const { return m_onePrSourceRepository; }
    QString onePrVariantRepository() const { return m_onePrVariantRepository; }
    QString pullRequestNumber() const { return m_pullRequestNumber; }
    bool generateApiJsonOnePr() const { return m_generateApiJsonOnePr; }
    bool fetchPrCommitListOnePr() const { return m_fetchPrCommitListOnePr; }

    void setOnePrSourceRepository(const QString &value);
    void setOnePrVariantRepository(const QString &value);
    void setPullRequestNumber(const QString &value);
    void setGenerateApiJsonOnePr(bool value);
    void setFetchPrCommitListOnePr(bool value);

    Q_INVOKABLE void submitOnePullRequest();

    // ---- Dataset batch -------------------------------------------------------
    QString datasetSources() const { return m_datasetSources; }
    QString datasetVariants() const { return m_datasetVariants; }
    bool generateApiJsonDataset() const { return m_generateApiJsonDataset; }
    bool fetchPrCommitListDataset() const { return m_fetchPrCommitListDataset; }
    bool runDatasetReversed() const { return m_runDatasetReversed; }
    bool skipRepoDeletionDataset() const { return m_skipRepoDeletionDataset; }
    bool datasetEndDateLimited() const { return m_datasetEndDateLimited; }
    QString datasetEndDateLimit() const;

    void setDatasetSources(const QString &value);
    void setDatasetVariants(const QString &value);
    void setGenerateApiJsonDataset(bool value);
    void setFetchPrCommitListDataset(bool value);
    void setRunDatasetReversed(bool value);
    void setSkipRepoDeletionDataset(bool value);
    void setDatasetEndDateLimited(bool value);
    void setDatasetEndDateLimit(const QString &value);

    Q_INVOKABLE void submitDataset();

    // ---- Dependencies --------------------------------------------------------
    bool dependencyInstallInProgress() const { return m_dependencyInstallInProgress; }
    QString dependencyProgressTitle() const { return m_dependencyProgressTitle; }
    QString dependencyProgressMessage() const { return m_dependencyProgressMessage; }

    Q_INVOKABLE void installDependencies();

    // ---- Classification progress ---------------------------------------------
    bool classificationProgressVisible() const { return m_classificationProgressVisible; }
    QString classificationProgressTitle() const { return m_classificationProgressTitle; }
    QString classificationStage() const { return m_classificationStage; }
    QAbstractListModel *classificationProgress() { return &m_classificationProgress; }

    // ---- Results viewer ------------------------------------------------------
    bool advancedResultsVisible() const { return m_advancedResultsVisible; }

    Q_INVOKABLE void openAdvancedResults();
    Q_INVOKABLE void openResultsDirectory(const QString &directoryUrl);
    Q_INVOKABLE void chooseResultsWindow(int index);

signals:
    void stateChanged();
    void dependencyProgressChanged();
    void classificationProgressChanged();
    void messageRequested(const QString &title,
                          const QString &message,
                          bool requiresAcknowledgement);
    void resultsViewerRequested(const QString &rootDirectory);
    void resultsWindowChoicesRequested(const QStringList &choices);

private:
    // ---- Dataset batch -------------------------------------------------------
    // Classifies every six-month window of every mainline/variant pair. Both
    // lists are updated in place with the canonical repository names GitHub
    // reports, and `tokens` is refreshed from tokens.txt between runs. Result
    // folders are named "<folderPrefix><run index>_<repo>_<from>_<to>".
    void runDatasetPairs(QStringList &mainlineRepos,
                         QStringList &variantRepos,
                         QStringList &tokens,
                         const QString &folderPrefix,
                         int firstRunIndex);

    // ---- Dependencies --------------------------------------------------------
    void ensureDependenciesOnLaunch();
    void installDependenciesWithProgress(bool automatic);

    // ---- Classification progress ---------------------------------------------
    void beginClassificationProgress();
    void connectProgressSignals(class GACPD *run);

    // ---- Running GACPD -------------------------------------------------------
    // Runs one classification on a worker thread and blocks on a local event
    // loop until it finishes, so callers can queue several runs back to back.
    void runGACPD(const QVariantList &params, bool parallel, bool onePr);

    // ---- Shared plumbing -----------------------------------------------------
    void showRunMessage(const QString &message);
    void requestResultsViewer(const QString &path);
    void loadPersistedTokens();
    void savePersistedTokens();

    // ---- Shared by every run mode --------------------------------------------
    QString m_projectName;
    QString m_tokens;

    // ---- Normal run ----------------------------------------------------------
    QString m_sourceRepository = QStringLiteral("apache/kafka");
    QString m_variantRepository = QStringLiteral("linkedin/kafka");

    // Kept as date/times rather than dates because the submit check compares
    // the two values directly.
    QDateTime m_divergenceDate = QDateTime::currentDateTime();
    QDateTime m_cutoffDate = QDateTime::currentDateTime();

    bool m_generateApiJsonNormal = false;
    bool m_fetchPrCommitListNormal = false;
    bool m_dateRangeVisible = false;
    bool m_normalSubmitVisible = false;

    // ---- Single pull request -------------------------------------------------
    QString m_onePrSourceRepository;
    QString m_onePrVariantRepository;
    QString m_pullRequestNumber;
    bool m_generateApiJsonOnePr = false;
    bool m_fetchPrCommitListOnePr = false;

    // ---- Dataset batch -------------------------------------------------------
    QString m_datasetSources;
    QString m_datasetVariants;
    bool m_generateApiJsonDataset = false;
    bool m_fetchPrCommitListDataset = false;
    bool m_runDatasetReversed = false;
    // Keep Repos_clones/Repos_files between runs and reuse them locally
    // instead of re-cloning every window from GitHub.
    bool m_skipRepoDeletionDataset = false;

    // Debug aid: when enabled, every pair stops at this date instead of at
    // whatever GitHub reports as its latest activity, so a whole dataset can
    // be re-run over exactly the same period.
    bool m_datasetEndDateLimited = false;
    QDate m_datasetEndDateLimit = QDate::currentDate();

    // ---- Dependencies --------------------------------------------------------
    bool m_dependencyInstallInProgress = false;
    QString m_dependencyProgressTitle;
    QString m_dependencyProgressMessage;

    // ---- Classification progress ---------------------------------------------
    bool m_classificationProgressVisible = false;
    QString m_classificationProgressTitle;
    QString m_classificationStage;
    ClassificationProgressModel m_classificationProgress;

    // A sequential run only ever shows the pull request it is working on; a
    // parallel one shows the whole queue.
    bool m_parallelProgress = true;

    // ---- Results viewer ------------------------------------------------------
    bool m_advancedResultsVisible = false;

    // Result folders offered while the "which six-month window?" dialog is
    // open, indexed by the choice list sent to QML.
    QStringList m_pendingResultWindows;
};

#endif // MAIN_WINDOW_H
