// Results viewer: choosing what to look at.
//
// Three selectors narrow the view down, each one refilling the next:
//
//     classification  ->  pull request  ->  file in the right pane
//
// plus a search box that can jump straight to a pull request by number, which
// means moving the classification selector for the user.

#include "headers/results/ResultsViewerController.h"
#include "headers/helpers/helpers.h"
#include "headers/helpers/prHelpers.h"
#include "headers/results/ResultFiles.h"
#include <QDebug>
#include <QFileInfo>
#include <algorithm>

static void refillIndividualResultsSelector(SelectionModel *selector,
                                            const QStringList &resultFiles){
    if (!selector)
        return;

    selector->blockSignals(true);
    const QString previousSelection =
        selector->currentValue().toString();
    selector->clear();

    QList<QPair<QString, QString>> labeledResults;
    labeledResults.reserve(resultFiles.size());

    for (const QString &resultsPath : resultFiles) {
        QString label = individualResultField(resultsPath, "fileName");
        if (label.isEmpty())
            label = QFileInfo(resultsPath).dir().dirName();
        labeledResults.append(qMakePair(label, resultsPath));
    }

    std::sort(labeledResults.begin(), labeledResults.end(),
              [](const QPair<QString, QString> &left,
                 const QPair<QString, QString> &right) {
        return left.first.compare(right.first, Qt::CaseInsensitive) < 0;
    });

    for (const QPair<QString, QString> &entry : labeledResults) {
        selector->addItem(entry.first, entry.second);
    }

    int newIndex = -1;
    if (!previousSelection.isEmpty())
        newIndex = selector->indexOfValue(previousSelection);

    if (newIndex < 0 && selector->count() > 0)
        newIndex = 0;

    selector->setCurrentIndex(newIndex);
    selector->blockSignals(false);
}

static void refillSelectorKeepSelection(SelectionModel *selector,
                                        const QStringList &files)
{
    if (!selector) {
        return;
    }

    selector->blockSignals(true);
    const QString previousSelection = selector->currentValue().toString();
    selector->clear();

    for (const QString &path : files) {
        const QString label = QFileInfo(path).fileName();
        selector->addItem(label, path);
    }

    int newIndex = previousSelection.isEmpty()
        ? -1
        : selector->indexOfValue(previousSelection);
    if (newIndex < 0 && selector->count() > 0) {
        newIndex = 0;
    }

    selector->setCurrentIndex(newIndex);
    selector->blockSignals(false);
}

ResultsViewerController::ResultsViewerController(QObject *parent, QString rootParam)
    : QObject(parent)
{
    // Treat the directory selected by the user as the authoritative result
    // root. Do not depend on where MOVis normally stores Repos_results; result
    // sets must remain viewable after being moved or renamed.
    createStateObjects();
    connectStateObjects();

    if (!rootParam.isEmpty()) {
        loadRootDirectory(rootParam);
    }
}

void ResultsViewerController::createStateObjects()
{
    classificationCombo = new SelectionModel(this);
    prCombo = new SelectionModel(this);
    leftFileSelector = new SelectionModel(this);
    leftFileSelector->addItem("ALL HUNKS SHOWING", 0);
    leftEdit = new CodeDocument(this);
    leftEdit->setOverrideLineNumbers(true);
    rightFileSelector = new SelectionModel(this);
    rightEdit = new CodeDocument(this);
    rightEdit->setOverrideLineNumbers(true);
}

void ResultsViewerController::connectStateObjects()
{
    connect(classificationCombo, &SelectionModel::currentIndexChanged,
            this, &ResultsViewerController::onClassificationChanged);
    connect(prCombo, &SelectionModel::currentIndexChanged,
            this, &ResultsViewerController::onPRChanged);
    connect(leftFileSelector, &SelectionModel::currentIndexChanged,
            this, &ResultsViewerController::onHunkViewChanged);
    connect(rightFileSelector, &SelectionModel::currentIndexChanged,
            this, &ResultsViewerController::switchFilesOnRight);
    connect(leftEdit, &CodeDocument::cursorPositionChanged,
            this, &ResultsViewerController::onCursorPositionChanged);

    connect(classificationCombo, &SelectionModel::changed,
            this, &ResultsViewerController::stateChanged);
    connect(prCombo, &SelectionModel::changed,
            this, &ResultsViewerController::stateChanged);
    connect(leftFileSelector, &SelectionModel::changed,
            this, &ResultsViewerController::stateChanged);
    connect(rightFileSelector, &SelectionModel::changed,
            this, &ResultsViewerController::stateChanged);

    connect(leftEdit, &CodeDocument::navigationRequested,
            this, &ResultsViewerController::leftNavigationRequested);
    connect(rightEdit, &CodeDocument::navigationRequested,
            this, &ResultsViewerController::rightNavigationRequested);
    connect(leftEdit, &CodeDocument::contentChanged,
            this, &ResultsViewerController::stateChanged);
    connect(rightEdit, &CodeDocument::contentChanged,
            this, &ResultsViewerController::stateChanged);
}

void ResultsViewerController::loadRootDirectory(const QString &path)
{
    rootDirectory = path.isEmpty()
        ? QString()
        : QFileInfo(path).absoluteFilePath();

    classificationToPRs.clear();
    if (!rootDirectory.isEmpty()) {
        scanRootFolderForPRsAndClasses(classificationToPRs, rootDirectory);
    }

    classificationCombo->blockSignals(true);
    classificationCombo->clear();
    for (const QString &classification : classificationOptions()) {
        if (classificationToPRs.contains(classification)) {
            classificationCombo->addItem(classification);
        }
    }
    classificationCombo->blockSignals(false);

    if (classificationCombo->count() > 0) {
        classificationCombo->setCurrentIndex(0);
    } else {
        prCombo->clear();
        rightFileSelector->clear();
        clearLeftPane(true);
    }
    emit stateChanged();
}

void ResultsViewerController::selectClassification(int index)
{
    classificationCombo->setCurrentIndex(index);
}

void ResultsViewerController::selectPullRequest(int index)
{
    prCombo->setCurrentIndex(index);
}

void ResultsViewerController::selectHunk(int index)
{
    leftFileSelector->setCurrentIndex(index);
}

void ResultsViewerController::selectRightFile(int index)
{
    rightFileSelector->setCurrentIndex(index);
}

QString ResultsViewerController::normalizePRSearchInput(const QString &text) const{
    QString pr = text.trimmed();

    if (pr.startsWith("PR", Qt::CaseInsensitive))
        pr = pr.mid(2).trimmed();

    if (pr.startsWith('#'))
        pr = pr.mid(1).trimmed();

    return pr;
}

QStringList ResultsViewerController::allPRNumbers() const{
    QSet<QString> all;

    for (const QSet<QString> &prs : classificationToPRs)
        all.unite(prs);

    return sortedSet(all);
}

QString ResultsViewerController::classificationForPR(const QString &pr) const{
    const QString currentClassification = classificationCombo ? classificationCombo->currentText() : QString();

    if (!currentClassification.isEmpty() && classificationToPRs.value(currentClassification).contains(pr))
        return currentClassification;

    QStringList classes = classificationToPRs.keys();
    classes.sort(Qt::CaseInsensitive);

    for (const QString &classification : classes) {
        if (classificationToPRs.value(classification).contains(pr))
            return classification;
    }

    return QString();
}

bool ResultsViewerController::selectPRByNumber(const QString &rawPR){
    const QString requestedPR = normalizePRSearchInput(rawPR);

    if (requestedPR.isEmpty())
        return false;

    QString selectedPR = requestedPR;
    QString targetClassification = classificationForPR(selectedPR);

    if (targetClassification.isEmpty()) {
        const QStringList prs = allPRNumbers();
        QStringList partialMatches;

        for (const QString &pr : prs) {
            if (pr.contains(selectedPR, Qt::CaseInsensitive))
                partialMatches.append(pr);
        }

        if (partialMatches.size() == 1) {
            selectedPR = partialMatches.first();
            targetClassification = classificationForPR(selectedPR);
        }
    }

    if (targetClassification.isEmpty()) {
        const QStringList prs = allPRNumbers();
        QStringList partialMatches;

        for (const QString &pr : prs) {
            if (pr.contains(requestedPR, Qt::CaseInsensitive))
                partialMatches.append(pr);
        }

        if (partialMatches.size() > 1) {
            emit messageRequested(
                "Multiple PR Matches",
                QString("Multiple PRs match '%1'. Choose one from autocomplete or type the full PR number.")
                    .arg(requestedPR));
        }
        else {
            emit messageRequested(
                "PR Not Found",
                QString("No PR matching '%1' was found in the loaded results folder.")
                    .arg(requestedPR));
        }

        return false;
    }

    const int classIndex = classificationCombo->indexOfLabel(targetClassification);
    if (classIndex < 0)
        return false;

    if (classificationCombo->currentIndex() != classIndex)
        classificationCombo->setCurrentIndex(classIndex);
    else
        onClassificationChanged(classIndex);

    const int prIndex = prCombo->indexOfLabel(selectedPR);
    if (prIndex < 0) {
        emit messageRequested(
            "PR Not Found",
            QString("PR '%1' was found, but it could not be selected under classification '%2'.")
                .arg(selectedPR, targetClassification));
        return false;
    }

    if (prCombo->currentIndex() != prIndex)
        prCombo->setCurrentIndex(prIndex);
    else
        onPRChanged(prIndex);

    // Store the canonical number so a partial search is remembered in its
    // resolved form.
    prSearchText = selectedPR;
    emit stateChanged();
    return true;
}

void ResultsViewerController::onClassificationChanged(int){
    clearSearchFields();
    setErrorDetails(QString());
    prCombo->blockSignals(true);
    prCombo->clear();

    const QString classification = classificationCombo->currentText();
    const QSet<QString> prsSet = classificationToPRs.value(classification);
    const QStringList prs = sortedSet(prsSet);

    setRightSelectorLabel(isTerminalVisualizerClassification(classification)
                              ? "Files"
                              : "File");


    prCombo->addItems(prs);
    prCombo->blockSignals(false);

    if (prCombo->count() > 0) {
        prCombo->setCurrentIndex(0);
        onPRChanged(0);
    } else {
        rightFileSelector->clear();
        currentLeftHunkFiles.clear();
        populateHunkViewSelector(currentLeftHunkFiles);
        leftEdit->clear();
        leftPath.clear();
        rightEdit->clear();
        rightPath.clear();

        setHunkLabel("-");
        setPRHyperlink("-", "");
        setPRResultsLink(QString());
        setFileResultsLink(QString());
    }
}

void ResultsViewerController::onPRChanged(int){
    clearSearchFields();
    setErrorDetails(QString());
    const QString classification = classificationCombo->currentText();
    const QString pr = prCombo->currentText();

    leftEdit->clear();
    leftPath.clear();
    currentLeftHunkFiles.clear();
    populateHunkViewSelector(currentLeftHunkFiles);
    setHunkLabel("-");
    setFileResultsLink(QString());

    const QString prResultsPath =
        prResultsFileForSelection(rootDirectory, pr, classification);
    setPRResultsLink(prResultsPath);

    if (!prResultsPath.isEmpty()) {
        QString error;
        if (!setSimilarityResultsFile(prResultsPath, &error)) {
            qWarning().noquote() << error;
            setPRHyperlink("-", "");
        }
    } else {
        similarityLoaded = false;
        similarityResultsPath.clear();
        setPRHyperlink("-", "");
    }

    if (isTerminalVisualizerClassification(classification)) {
        const QStringList resultFiles = scanIndividualResultFiles(
            rootDirectory, pr, classification, classification);
        refillIndividualResultsSelector(rightFileSelector, resultFiles);
    } else {
        const QStringList cmpFiles =
            scallAllFilesInCmp(rootDirectory, pr, classification);
        refillSelectorKeepSelection(rightFileSelector, cmpFiles);
    }

    if (rightFileSelector->currentIndex() >= 0) {
        switchFilesOnRight(rightFileSelector->currentIndex());
    } else {
        rightEdit->clear();
        rightPath.clear();
        setFileResultsLink(QString());
    }
}

void ResultsViewerController::searchPullRequest(const QString &text)
{
    prSearchText = text;
    selectPRByNumber(text);
}

void ResultsViewerController::clearPullRequestSearch()
{
    prSearchText.clear();
    emit stateChanged();
}
