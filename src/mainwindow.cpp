#include "headers/mainwindow.h"

#include "headers/GACPD/GACPD.h"
#include "headers/GACPD/Methods/appPath.h"
#include "headers/GACPD/Methods/helpers_GACPD.h"
#include "headers/helpers/dateWindows.h"
#include "headers/helpers/dependencySetup.h"
#include "headers/helpers/githubHelpers.h"
#include "headers/helpers/resultFolders.h"
#include "headers/helpers/tokenStore.h"
#include "headers/helpers/userInput.h"

#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QRegularExpression>
#include <QSettings>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QtConcurrent/QtConcurrent>
#include <memory>
#include <utility>

MainWindow::MainWindow(QObject *parent)
    : QObject(parent)
{
    loadPersistedTokens();

    // On first launch, or after node_modules has been removed, install the
    // bundled JavaScript dependencies. The Dependencies button stays available
    // as a manual fallback.
    QTimer::singleShot(0, this, &MainWindow::ensureDependenciesOnLaunch);
}

// ===========================================================================
// Shared by every run mode
// ===========================================================================

QString MainWindow::resultsRoot() const
{
    return movisResultsRoot();
}

void MainWindow::setProjectName(const QString &value)
{
    if (m_projectName == value) {
        return;
    }
    m_projectName = value;
    emit stateChanged();
}

void MainWindow::setTokens(const QString &value)
{
    if (m_tokens == value) {
        return;
    }
    m_tokens = value;
    emit stateChanged();
}

void MainWindow::saveSettings()
{
    savePersistedTokens();
}

// ===========================================================================
// Normal run: one source/variant pair over a date range
// ===========================================================================

QString MainWindow::divergenceDate() const
{
    return m_divergenceDate.date().toString(Qt::ISODate);
}

QString MainWindow::cutoffDate() const
{
    return m_cutoffDate.date().toString(Qt::ISODate);
}

void MainWindow::setSourceRepository(const QString &value)
{
    if (m_sourceRepository == value) {
        return;
    }
    m_sourceRepository = value;
    emit stateChanged();
}

void MainWindow::setVariantRepository(const QString &value)
{
    if (m_variantRepository == value) {
        return;
    }
    m_variantRepository = value;
    emit stateChanged();
}

void MainWindow::setDivergenceDate(const QString &value)
{
    // Only the date is editable. The time of day is kept so the submit-time
    // comparison against the cut-off date stays consistent.
    const QDate date = QDate::fromString(value.left(10), Qt::ISODate);
    if (!date.isValid() || date == m_divergenceDate.date()) {
        return;
    }

    m_divergenceDate.setDate(date);
    emit stateChanged();
}

void MainWindow::setCutoffDate(const QString &value)
{
    const QDate date = QDate::fromString(value.left(10), Qt::ISODate);
    if (!date.isValid() || date == m_cutoffDate.date()) {
        return;
    }

    m_cutoffDate.setDate(date);
    emit stateChanged();
}

void MainWindow::setGenerateApiJsonNormal(bool value)
{
    if (m_generateApiJsonNormal == value) {
        return;
    }
    m_generateApiJsonNormal = value;
    emit stateChanged();
}

void MainWindow::setFetchPrCommitListNormal(bool value)
{
    if (m_fetchPrCommitListNormal == value) {
        return;
    }
    m_fetchPrCommitListNormal = value;
    emit stateChanged();
}

void MainWindow::checkDivergenceDate()
{
    if (m_sourceRepository.isEmpty() || m_variantRepository.isEmpty()) {
        showRunMessage("Source or Target Variant are empty. Try again.");
        return;
    }

    // The divergence lookup takes the raw lines: it only needs something to
    // authenticate with, and the tokens are normalized before the actual run.
    static const QRegularExpression lineBreaks("[\r\n]+");
    const QStringList tokenList =
        m_tokens.split(lineBreaks, Qt::SkipEmptyParts);

    if (tokenList.isEmpty()) {
        showRunMessage("You need at least one token. Try again.");
        return;
    }

    const DivergenceResult result = divergence_date_qt(
        removeInvisibleCharacters(m_sourceRepository).trimmed(),
        removeInvisibleCharacters(m_variantRepository).trimmed(),
        tokenList,
        0);

    if (result.divergeDate.trimmed().isEmpty()) {
        m_dateRangeVisible = false;
        m_normalSubmitVisible = false;
        emit stateChanged();
        showRunMessage("Divergence Date is unobtainable");
        return;
    }

    // Use GitHub's canonical names from this point forward. Both the source
    // and the variant repository may have been renamed or transferred.
    if (!result.mainlineRepo.trimmed().isEmpty()) {
        m_sourceRepository = result.mainlineRepo.trimmed();
    }
    if (!result.variantRepo.trimmed().isEmpty()) {
        m_variantRepository = result.variantRepo.trimmed();
    }

    m_divergenceDate.setDate(
        QDateTime::fromString(result.divergeDate, Qt::ISODate).date());
    m_cutoffDate.setDate(
        QDateTime::fromString(result.leastDate, Qt::ISODate).date());

    m_dateRangeVisible = true;
    m_normalSubmitVisible = true;
    emit stateChanged();
}

void MainWindow::overrideDivergenceDate()
{
    if (m_sourceRepository.isEmpty() || m_variantRepository.isEmpty()) {
        showRunMessage("Source or Target Variant are empty. Try again.");
        return;
    }

    static const QRegularExpression lineBreaks("[\r\n]+");
    const QStringList tokenList =
        m_tokens.split(lineBreaks, Qt::SkipEmptyParts);

    if (tokenList.isEmpty()) {
        showRunMessage("You need at least one token. Try again.");
        return;
    }

    const DivergenceResult result = detached_pair_dates_qt(
        removeInvisibleCharacters(m_sourceRepository).trimmed(),
        removeInvisibleCharacters(m_variantRepository).trimmed(),
        tokenList);

    if (result.divergeDate.trimmed().isEmpty() ||
        result.leastDate.trimmed().isEmpty()) {
        m_dateRangeVisible = false;
        m_normalSubmitVisible = false;
        emit stateChanged();
        showRunMessage("Could not look up one of the repositories. "
                       "Check both names and your tokens.");
        return;
    }

    if (!result.mainlineRepo.trimmed().isEmpty()) {
        m_sourceRepository = result.mainlineRepo.trimmed();
    }
    if (!result.variantRepo.trimmed().isEmpty()) {
        m_variantRepository = result.variantRepo.trimmed();
    }

    m_divergenceDate.setDate(
        QDateTime::fromString(result.divergeDate, Qt::ISODate).date());
    m_cutoffDate.setDate(
        QDateTime::fromString(result.leastDate, Qt::ISODate).date());

    m_dateRangeVisible = true;
    m_normalSubmitVisible = true;
    emit stateChanged();

    showRunMessage(QString("Detached pair override: no shared fork history, so the "
                           "divergence date was set to the variant's creation date "
                           "(%1). Adjust the date range if needed.")
                       .arg(m_divergenceDate.date().toString(Qt::ISODate)));
}

void MainWindow::submitNormal()
{
    const QString sourceRepo = removeInvisibleCharacters(m_sourceRepository).trimmed().remove(' ');
    const QString variantRepo = removeInvisibleCharacters(m_variantRepository).trimmed().remove(' ');
    const QString projectName = removeInvisibleCharacters(m_projectName).trimmed().remove(' ');

    if (projectName.isEmpty()) {
        showRunMessage("Empty Results Folder Name - Please provide a name.");
        return;
    }

    // Project names have to stay unique so a second run cannot merge its
    // results into an existing set.
    if (projectHasResults(movisResultsRoot() + "/" + projectName)) {
        showRunMessage("Results folders must be unique - please give us a new one.");
        return;
    }

    if (sourceRepo.isEmpty() || variantRepo.isEmpty()) {
        showRunMessage("Source or Target Variant are empty. Try again.");
        return;
    }

    if (m_tokens.isEmpty()) {
        showRunMessage("You need at least 1 token to run the program.");
        return;
    }

    if (!m_divergenceDate.isValid() || !m_cutoffDate.isValid()) {
        showRunMessage("Divergence dates are empty.");
        return;
    }

    if (m_cutoffDate < m_divergenceDate) {
        showRunMessage("Divergence Date is less than Initial Date.");
        return;
    }

    const QList<DateWindow> windows =
        splitIntoSixMonthWindows(m_divergenceDate, m_cutoffDate);

    if (windows.isEmpty()) {
        showRunMessage("Could not create a valid date window for this run.");
        return;
    }

    const QStringList tokenList = parseGithubTokenLines(m_tokens);
    qDebug().noquote() << "Normal run:" << sourceRepo << "->" << variantRepo
                       << "into" << projectName
                       << "with" << tokenList.size() << "tokens";

    // Tokens are also saved on close; save now so a long run cannot lose them.
    savePersistedTokens();

    int windowIndex = 1;
    for (const DateWindow &window : windows) {
        // A single-window run keeps the legacy single-folder layout; a longer
        // one writes one sibling folder per window.
        const QString runRoot = windows.size() > 1
            ? sixMonthSliceName(projectName, windowIndex, window)
            : projectName;

        qDebug().noquote() << "  window" << windowIndex << ":"
                           << window.start.toString(Qt::ISODate)
                           << "->" << window.end.toString(Qt::ISODate);

        try {
            runGACPD({runRoot,
                      sourceRepo,
                      variantRepo,
                      tokenList,
                      isoStartOfDay(window.start),
                      isoEndOfDay(window.end),
                      m_generateApiJsonNormal,
                      m_fetchPrCommitListNormal},
                     /*parallel=*/true,
                     /*onePr=*/false);
        } catch (...) {
            showRunMessage("Failed to open results. Look at logs for more information.");
        }

        ++windowIndex;
    }
}

// ===========================================================================
// Single pull request
// ===========================================================================

void MainWindow::setOnePrSourceRepository(const QString &value)
{
    if (m_onePrSourceRepository == value) {
        return;
    }
    m_onePrSourceRepository = value;
    emit stateChanged();
}

void MainWindow::setOnePrVariantRepository(const QString &value)
{
    if (m_onePrVariantRepository == value) {
        return;
    }
    m_onePrVariantRepository = value;
    emit stateChanged();
}

void MainWindow::setPullRequestNumber(const QString &value)
{
    if (m_pullRequestNumber == value) {
        return;
    }
    m_pullRequestNumber = value;
    emit stateChanged();
}

void MainWindow::setGenerateApiJsonOnePr(bool value)
{
    if (m_generateApiJsonOnePr == value) {
        return;
    }
    m_generateApiJsonOnePr = value;
    emit stateChanged();
}

void MainWindow::setFetchPrCommitListOnePr(bool value)
{
    if (m_fetchPrCommitListOnePr == value) {
        return;
    }
    m_fetchPrCommitListOnePr = value;
    emit stateChanged();
}

void MainWindow::submitOnePullRequest()
{
    const QString sourceRepo = removeInvisibleCharacters(m_onePrSourceRepository).trimmed().remove(' ');
    const QString variantRepo = removeInvisibleCharacters(m_onePrVariantRepository).trimmed().remove(' ');
    const QString prNumber = removeInvisibleCharacters(m_pullRequestNumber).trimmed().remove(' ');
    const QString projectName = removeInvisibleCharacters(m_projectName).trimmed().remove(' ');
    const QStringList tokenList = parseGithubTokenLines(m_tokens);

    if (sourceRepo.isEmpty() || variantRepo.isEmpty()) {
        showRunMessage("Source or Variant repos are empty. Try again.");
        return;
    }

    if (projectName.isEmpty()) {
        showRunMessage("We must have a root folder to save into.");
        return;
    }

    if (prNumber.isEmpty()) {
        showRunMessage("We must have a pr number to classify.");
        return;
    }

    if (tokenList.isEmpty()) {
        showRunMessage("You need at least 1 token to run the program.");
        return;
    }

    qDebug().noquote() << "Single PR run:" << sourceRepo << "->" << variantRepo
                       << "PR" << prNumber
                       << "with" << tokenList.size() << "tokens";

    try {
        // A single-PR run resolves its own date range, so both bounds are
        // left empty here.
        runGACPD({"onePr_" + projectName,
                  sourceRepo,
                  variantRepo,
                  tokenList,
                  QString(""),
                  QString(""),
                  m_generateApiJsonOnePr,
                  m_fetchPrCommitListOnePr},
                 /*parallel=*/false,
                 /*onePr=*/true);
    } catch (...) {
        showRunMessage("Failed to open results. Look at logs for more information.");
    }
}

// ===========================================================================
// Dataset batch: many source/variant pairs in one go
// ===========================================================================

void MainWindow::setDatasetSources(const QString &value)
{
    if (m_datasetSources == value) {
        return;
    }
    m_datasetSources = value;
    emit stateChanged();
}

void MainWindow::setDatasetVariants(const QString &value)
{
    if (m_datasetVariants == value) {
        return;
    }
    m_datasetVariants = value;
    emit stateChanged();
}

void MainWindow::setGenerateApiJsonDataset(bool value)
{
    if (m_generateApiJsonDataset == value) {
        return;
    }
    m_generateApiJsonDataset = value;
    emit stateChanged();
}

void MainWindow::setFetchPrCommitListDataset(bool value)
{
    if (m_fetchPrCommitListDataset == value) {
        return;
    }
    m_fetchPrCommitListDataset = value;
    emit stateChanged();
}

void MainWindow::setSkipRepoDeletionDataset(bool value)
{
    if (m_skipRepoDeletionDataset == value) {
        return;
    }
    m_skipRepoDeletionDataset = value;
    emit stateChanged();
}

void MainWindow::setRunDatasetReversed(bool value)
{
    if (m_runDatasetReversed == value) {
        return;
    }
    m_runDatasetReversed = value;
    emit stateChanged();
}

QString MainWindow::datasetEndDateLimit() const
{
    return m_datasetEndDateLimit.toString(Qt::ISODate);
}

void MainWindow::setDatasetEndDateLimited(bool value)
{
    if (m_datasetEndDateLimited == value) {
        return;
    }
    m_datasetEndDateLimited = value;
    emit stateChanged();
}

void MainWindow::setDatasetEndDateLimit(const QString &value)
{
    const QDate date = QDate::fromString(value.left(10), Qt::ISODate);
    if (!date.isValid() || date == m_datasetEndDateLimit) {
        return;
    }

    m_datasetEndDateLimit = date;
    emit stateChanged();
}

void MainWindow::submitDataset()
{
    QStringList tokenList = splitOnWhitespace(m_tokens);
    QStringList sourceRepos = splitOnWhitespace(m_datasetSources);
    QStringList variantRepos = splitOnWhitespace(m_datasetVariants);

    for (QString &token : tokenList) {
        token = normalizeGithubToken(token);
    }
    tokenList.removeAll(QString());

    qDebug().noquote() << "Dataset batch:" << sourceRepos.size() << "pairs,"
                       << tokenList.size() << "tokens";

    // Tidy the three boxes back to one item per line so the user sees exactly
    // what was understood.
    setTokens(tokenList.join('\n'));
    setDatasetSources(sourceRepos.join('\n'));
    setDatasetVariants(variantRepos.join('\n'));

    QString projectName = m_projectName;
    projectName.remove(QRegularExpression("\\s+"));

    if (projectName.isEmpty()) {
        showRunMessage("We must have a root folder to save into.");
        return;
    }

    if (tokenList.isEmpty()) {
        showRunMessage("You need at least 1 token to run the program.");
        return;
    }

    if (sourceRepos.isEmpty() || variantRepos.isEmpty()) {
        showRunMessage("You need at least 1 source/variant repo to run the program.");
        return;
    }

    if (sourceRepos.size() != variantRepos.size()) {
        showRunMessage("The number of pairs between source and variants must be equal.");
        return;
    }

    runDatasetPairs(sourceRepos, variantRepos, tokenList,
                    "d" + projectName + "_", 1);

    // Reflect any GitHub rename or transfer the runs resolved.
    setDatasetSources(sourceRepos.join('\n'));
    setDatasetVariants(variantRepos.join('\n'));

    if (m_runDatasetReversed) {
        // The same pairs with the roles swapped: each variant is treated as
        // the mainline and vice versa.
        runDatasetPairs(variantRepos, sourceRepos, tokenList,
                        "d" + projectName + "R_", 0);
    }
}

void MainWindow::runDatasetPairs(QStringList &mainlineRepos,
                                 QStringList &variantRepos,
                                 QStringList &tokens,
                                 const QString &folderPrefix,
                                 int firstRunIndex)
{
    int runIndex = firstRunIndex;

    for (int pair = 0; pair < mainlineRepos.size(); ++pair) {
        DivergenceResult result = divergence_date_qt(mainlineRepos[pair],
                                                     variantRepos[pair],
                                                     tokens,
                                                     0);

        // Detached pairs (outside one fork network) have no merge base, so
        // fall back to the metadata estimate the Normal page's override uses.
        if (result.divergeDate.trimmed().isEmpty()) {
            qWarning().noquote()
                << QString("Dataset pair %1 -> %2: no merge base; treating it as a "
                           "detached pair and estimating dates from repository metadata.")
                       .arg(mainlineRepos[pair], variantRepos[pair]);
            result = detached_pair_dates_qt(mainlineRepos[pair],
                                            variantRepos[pair],
                                            tokens);
        }

        if (result.divergeDate.trimmed().isEmpty()) {
            qWarning().noquote()
                << QString("Skipping dataset pair %1 -> %2: divergence date is unobtainable.")
                       .arg(mainlineRepos[pair], variantRepos[pair]);
            continue;
        }

        // Adopt the canonical names GitHub reports for renamed or transferred
        // repositories so the result folders stay consistent.
        if (!result.mainlineRepo.trimmed().isEmpty()) {
            mainlineRepos[pair] = result.mainlineRepo.trimmed();
        }
        if (!result.variantRepo.trimmed().isEmpty()) {
            variantRepos[pair] = result.variantRepo.trimmed();
        }

        const QDateTime divergence =
            QDateTime::fromString(result.divergeDate, Qt::ISODate);
        QDate lastDay =
            QDateTime::fromString(result.leastDate, Qt::ISODate).date();

        // With the debug limit on, no pair is allowed past the fixed end date,
        // so nothing beyond it is ever requested from GitHub.
        if (m_datasetEndDateLimited && m_datasetEndDateLimit.isValid()
            && (!lastDay.isValid() || m_datasetEndDateLimit < lastDay)) {
            lastDay = m_datasetEndDateLimit;
        }

        const QList<DateWindow> windows = splitIntoSixMonthWindows(
            divergence, QDateTime(lastDay, QTime(0, 0)));

        if (windows.isEmpty()) {
            qWarning().noquote()
                << QString("Skipping dataset pair %1 -> %2: nothing to classify up to %3.")
                       .arg(mainlineRepos[pair], variantRepos[pair],
                            lastDay.toString(Qt::ISODate));
            continue;
        }

        for (const DateWindow &window : windows) {
            const QString runRoot = sanitizedFolderName(
                folderPrefix
                + QString::number(runIndex) + "_"
                + mainlineRepos[pair] + "_"
                + window.start.toString(Qt::ISODate) + "_"
                + window.end.toString(Qt::ISODate));

            try {
                runGACPD({runRoot,
                          mainlineRepos[pair],
                          variantRepos[pair],
                          tokens,
                          isoStartOfDay(window.start),
                          isoEndOfDay(window.end),
                          m_generateApiJsonDataset,
                          m_fetchPrCommitListDataset,
                          m_skipRepoDeletionDataset},
                         /*parallel=*/true,
                         /*onePr=*/false);

                // A long batch can outlive its tokens, so pick up any
                // replacements the user wrote to tokens.txt in the meantime.
                tokens = loadTokensFile(tokens);
            } catch (...) {
                showRunMessage("Failed to open results. Look at logs for more information.");
            }

            ++runIndex;
        }
    }
}

// ===========================================================================
// Dependencies
// ===========================================================================

void MainWindow::installDependencies()
{
    installDependenciesWithProgress(/*automatic=*/false);
}

void MainWindow::ensureDependenciesOnLaunch()
{
    // The pinned JSCPD normally ships inside the application. On macOS the
    // manual fallback installs beside it instead, so check both.
    const bool bundled = hasPinnedJscpd(movisLocalRoot());
    const bool installed = hasPinnedJscpd(dependencyInstallRoot());

    if (bundled || installed) {
        qDebug() << "Pinned JSCPD" << pinnedJscpdVersion()
                 << "found; skipping automatic installation.";
        return;
    }

    qDebug() << "Pinned JSCPD" << pinnedJscpdVersion()
             << "not found; installing into" << dependencyInstallRoot();

    installDependenciesWithProgress(/*automatic=*/true);
}

void MainWindow::installDependenciesWithProgress(bool automatic)
{
    if (m_dependencyInstallInProgress) {
        emit messageRequested(
            "Dependency Installation In Progress",
            "MOVis is already installing the required dependencies. Please wait for it to finish.",
            true);
        return;
    }

    m_dependencyInstallInProgress = true;
    m_dependencyProgressTitle = automatic
        ? QStringLiteral("Installing Required Dependencies")
        : QStringLiteral("Installing Dependencies");
    m_dependencyProgressMessage = automatic
        ? QStringLiteral("Required dependencies were not found.\n"
                         "MOVis is installing them now. This may take a few minutes...")
        : QStringLiteral("Installing MOVis dependencies.\n"
                         "This may take a few minutes...");
    emit dependencyProgressChanged();

    const QString installRoot = dependencyInstallRoot();

#ifdef Q_OS_MACOS
    // Never install into Contents/Resources at runtime: that would invalidate
    // the bundle signature. Copy the npm metadata to the writable location and
    // let the bundled Node run the install there.
    QDir().mkpath(installRoot);
    const QString writablePackage = QDir(installRoot).filePath("package.json");
    const QString writableLock = QDir(installRoot).filePath("package-lock.json");

    QFile::remove(writablePackage);
    QFile::copy(movisPath("package.json"), writablePackage);

    if (QFileInfo::exists(movisPath("package-lock.json"))) {
        QFile::remove(writableLock);
        QFile::copy(movisPath("package-lock.json"), writableLock);
    }
#endif

    QFutureWatcher<bool> *watcher = new QFutureWatcher<bool>(this);

    connect(watcher, &QFutureWatcher<bool>::finished, this,
            [this, watcher, automatic]() {
                const bool success = watcher->result();
                m_dependencyInstallInProgress = false;
                emit dependencyProgressChanged();
                watcher->deleteLater();

                if (success) {
                    emit messageRequested(
                        "Dependencies Installed Successfully",
                        automatic
                            ? "Required dependencies were installed successfully. MOVis is ready to use."
                            : "Dependencies were installed successfully.",
                        true);
                    return;
                }

                emit messageRequested(
                    automatic ? "Automatic Dependency Installation Failed"
                              : "Dependency Installation Failed",
                    automatic
                        ? "node_modules was not found and MOVis could not install the dependencies "
                          "automatically. You can retry using the Dependencies action."
                        : "MOVis was unable to install the dependencies. Check that the bundled Node/npm "
                          "files are available for your operating system, then try again.",
                    true);
            });

    watcher->setFuture(QtConcurrent::run([installRoot]() {
        return setupJscpd(installRoot);
    }));
}

// ===========================================================================
// Results viewer
// ===========================================================================

void MainWindow::openAdvancedResults()
{
    requestResultsViewer(movisResultsPath(removeInvisibleCharacters(m_projectName).trimmed()));
}

void MainWindow::openResultsDirectory(const QString &directoryUrl)
{
    const QUrl url(directoryUrl);
    const QString directory = url.isLocalFile() ? url.toLocalFile()
                                                : directoryUrl;

    if (directory.trimmed().isEmpty()) {
        emit messageRequested("No Directory Selected",
                              "You must select a directory before continuing.",
                              true);
        return;
    }

    requestResultsViewer(directory);
}

void MainWindow::requestResultsViewer(const QString &path)
{
    m_pendingResultWindows = resultWindowsUnder(path);

    if (m_pendingResultWindows.isEmpty()) {
        return;
    }

    if (m_pendingResultWindows.size() == 1) {
        emit resultsViewerRequested(m_pendingResultWindows.constFirst());
        m_pendingResultWindows.clear();
        return;
    }

    // Several windows to choose from: ask, then continue in
    // chooseResultsWindow().
    QStringList labels;
    labels.reserve(m_pendingResultWindows.size());
    for (const QString &resultPath : std::as_const(m_pendingResultWindows)) {
        labels.append(QFileInfo(resultPath).fileName());
    }
    emit resultsWindowChoicesRequested(labels);
}

void MainWindow::chooseResultsWindow(int index)
{
    if (index < 0 || index >= m_pendingResultWindows.size()) {
        m_pendingResultWindows.clear();
        return;
    }

    const QString selectedPath = m_pendingResultWindows.at(index);
    m_pendingResultWindows.clear();
    emit resultsViewerRequested(selectedPath);
}

// ===========================================================================
// Token storage
// ===========================================================================

void MainWindow::loadPersistedTokens()
{
    QSettings settings;  // organization and application come from main.cpp
    m_tokens = settings.value("tokens/plainText", "").toString();
}

void MainWindow::savePersistedTokens()
{
    QSettings settings;
    settings.setValue("tokens/plainText", m_tokens);

    QString error;
    if (!saveTokensFile(m_tokens, &error)) {
        emit messageRequested("Save Error", error, true);
    }
}

// ===========================================================================
// Classification progress
// ===========================================================================

void MainWindow::beginClassificationProgress()
{
    m_classificationProgress.clear();
    m_classificationProgressVisible = true;
    m_classificationProgressTitle = "Running GACPD Classification";
    m_classificationStage = "Waiting to start...";
    emit classificationProgressChanged();
}

void MainWindow::connectProgressSignals(GACPD *run)
{
    connect(run, &GACPD::classificationStarted, this,
            [this](const QString &title) {
                m_classificationProgressTitle = title;
                m_classificationStage = "Starting...";
                m_classificationProgressVisible = true;
                m_classificationProgress.clear();
                emit classificationProgressChanged();
            }, Qt::QueuedConnection);

    connect(run, &GACPD::stageChanged, this,
            [this](const QString &message) {
                m_classificationStage = message;
                emit classificationProgressChanged();
            }, Qt::QueuedConnection);

    connect(run, &GACPD::stageFinished, this,
            [this](const QString &message) {
                m_classificationStage = message;
                emit classificationProgressChanged();
            }, Qt::QueuedConnection);

    connect(run, &GACPD::classifyStarted, this,
            [this](bool isParallel) {
                m_parallelProgress = isParallel;
                m_classificationProgress.clear();
            }, Qt::QueuedConnection);

    connect(run, &GACPD::prQueued, this,
            [this](const QString &prNumber, int totalFiles) {
                // A sequential run only shows the pull request it is working
                // on, so ignore the rest of the queue.
                if (!m_parallelProgress
                    && !m_classificationProgress.isEmpty()
                    && !m_classificationProgress.contains(prNumber)) {
                    return;
                }
                m_classificationProgress.setProgress(prNumber, 0, totalFiles,
                                                     "waiting", false);
            }, Qt::QueuedConnection);

    connect(run, &GACPD::prStarted, this,
            [this](const QString &prNumber) {
                if (!m_parallelProgress) {
                    m_classificationProgress.clear();
                }
                m_classificationProgress.setProgress(
                    prNumber,
                    m_classificationProgress.completedFilesFor(prNumber),
                    m_classificationProgress.totalFilesFor(prNumber),
                    "running", false);
            }, Qt::QueuedConnection);

    connect(run, &GACPD::prFileProgress, this,
            [this](const QString &prNumber, int completed, int total) {
                m_classificationProgress.setProgress(prNumber, completed, total,
                                                     "running", false);
            }, Qt::QueuedConnection);

    connect(run, &GACPD::prFinished, this,
            [this](const QString &prNumber) {
                const int total = m_classificationProgress.totalFilesFor(prNumber);
                m_classificationProgress.setProgress(prNumber, total, total,
                                                     "finished", true);
            }, Qt::QueuedConnection);

    connect(run, &GACPD::classificationFinished, this,
            [this]() {
                m_classificationStage = "Finished.";
                m_classificationProgressVisible = false;
                emit classificationProgressChanged();
            }, Qt::QueuedConnection);
}

// ===========================================================================
// Running GACPD
// ===========================================================================

void MainWindow::runGACPD(const QVariantList &params,
                          bool parallel,
                          bool onePr)
{
    GACPD *run = new GACPD(params);
    QThread *workerThread = new QThread(this);
    std::shared_ptr<QElapsedTimer> workerTimer = std::make_shared<QElapsedTimer>();

    // Only a single-PR run reads this list; the other modes discover their own
    // pull requests from the date range.
    const QStringList selectedPrs = {removeInvisibleCharacters(m_pullRequestNumber).trimmed()};

    beginClassificationProgress();
    connectProgressSignals(run);
    run->moveToThread(workerThread);

    connect(workerThread, &QThread::started, this,
            [run, workerTimer, selectedPrs, parallel, onePr]() {
                workerTimer->start();
                QMetaObject::invokeMethod(
                    run,
                    [run, selectedPrs, parallel, onePr]() {
                        run->runClassification(
                            selectedPrs,
                            "Running GACPD Classification",
                            parallel,
                            onePr);
                    },
                    Qt::QueuedConnection);
            });

    connect(run, &GACPD::classificationFinished,
            workerThread, &QThread::quit,
            Qt::QueuedConnection);

    connect(workerThread, &QThread::finished, this,
            [this, workerTimer]() {
                const qint64 totalSeconds = workerTimer->elapsed() / 1000;
                qDebug().noquote()
                    << QString("Worker thread total time: %1:%2:%3")
                           .arg(totalSeconds / 3600, 2, 10, QChar('0'))
                           .arg((totalSeconds % 3600) / 60, 2, 10, QChar('0'))
                           .arg(totalSeconds % 60, 2, 10, QChar('0'));

                m_advancedResultsVisible = true;
                emit stateChanged();
            });

    connect(workerThread, &QThread::finished,
            run, &QObject::deleteLater);
    connect(workerThread, &QThread::finished,
            workerThread, &QObject::deleteLater);

    // Callers queue runs one after another, so wait here for this one to end
    // while the interface stays responsive on the nested event loop.
    QEventLoop runLoop;
    connect(workerThread, &QThread::finished,
            &runLoop, &QEventLoop::quit);

    workerThread->start();
    runLoop.exec();
}

// ===========================================================================
// Shared plumbing
// ===========================================================================

void MainWindow::showRunMessage(const QString &message)
{
    emit messageRequested("Current Run Information", message, true);
}
