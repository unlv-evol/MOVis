// Results viewer: the header strip above the two panes.
//
// Everything here writes into the one ResultsContext struct the QML reads:
// the pull request title and link, links to the result files behind the
// current selection, and the classification of the selected file and hunk.

#include "headers/results/ResultsViewerController.h"
#include "headers/helpers/prHelpers.h"
#include "headers/results/ResultFiles.h"
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

void ResultsViewerController::setPRHyperlink(const QString &title, const QString &locationUrl){
    context.prTitle = title.isEmpty() ? QStringLiteral("-") : title;
    context.prUrl = locationUrl.isEmpty() ? QStringLiteral("-") : locationUrl;
    emit stateChanged();
}

void ResultsViewerController::setPRResultsLink(const QString &filePath){
    context.prResultsPath = existingResultsFilePath(filePath);
    emit stateChanged();
}

void ResultsViewerController::setFileResultsLink(const QString &filePath){
    context.fileResultsPath = existingResultsFilePath(filePath);
    emit stateChanged();
}

void ResultsViewerController::setRightSelectorLabel(const QString &text){
    context.rightSelectorLabel = text;
    emit stateChanged();
}

bool ResultsViewerController::setSimilarityResultsFile(const QString &resultsPath, QString *errorOut){
    similarityResultsPath = resultsPath;
    similarityLoaded = false;

    if (resultsPath.isEmpty()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Empty similarity results path");
        }
        return false;
    }

    QString localError;
    if (!similarityParser.load(resultsPath, &localError)) {
        if (errorOut) {
            *errorOut = localError;
        } else {
            qWarning().noquote() << localError;
        }
        return false;
    }

    similarityLoaded = true;
    setPRHyperlink(similarityParser.getPRTitle(), similarityParser.getPRLocation());
    return true;
}

bool ResultsViewerController::querySimilarity(const QString &fileName,
                                    const QString &hunkName,
                                    double *similarityOut,
                                    QString *classificationOut){
    if (!similarityLoaded) {
        qWarning() << "Similarity results not loaded (call setSimilarityResultsFile first)";
        return false;
    }

    return similarityParser.query(fileName, hunkName,
                                    similarityOut, classificationOut);
}

void ResultsViewerController::updateSimilarityOnPair(){
    // We need both sides to be loaded to do anything meaningful.
    if (rightPath.isEmpty()) {
        return;
    }

    // Result sets are portable. Resolve the PR metadata from the result root
    // selected by the user rather than searching the absolute path for the
    // literal directory name "Repos_results".
    const QString classification = classificationCombo->currentText();
    const QString pr = prCombo->currentText();
    const QString simPath =
        prResultsFileForSelection(rootDirectory, pr, classification);

    if (simPath.isEmpty()) {
        return;
    }

    // Load similarity file only once per PR or when the path changes.
    QString err;
    if (!similarityLoaded || similarityResultsPath != simPath) {
        setSimilarityResultsFile(simPath, &err);
    }
}

void ResultsViewerController::setErrorDetails(const QString &text){
    const QString cleaned = text.trimmed();

    context.errorDetailsVisible = !cleaned.isEmpty();
    context.errorDetails = cleaned.isEmpty() ? QStringLiteral("-") : cleaned;
    emit stateChanged();
}

void ResultsViewerController::updateSelectedHunkClassificationDisplay(){
    if (selectedHunkNumber <= 0) {
        context.hunkSideClassificationsVisible = false;
        context.hunkSideClassifications =
            QStringLiteral("Hunk_Add_Class: - | Hunk_Del_Class: -");
        emit stateChanged();
        return;
    }

    QString addClass = "-";
    QString delClass = "-";

    const QString classification = classificationCombo
        ? classificationCombo->currentText()
        : QString();
    const QString pr = prCombo ? prCombo->currentText() : QString();
    const QString selectedPath = rightFileSelector
        ? rightFileSelector->currentValue().toString()
        : QString();

    const QString resultsPath = individualResultsForSelectedPath(
        rootDirectory, pr, classification, selectedPath);

    QFile file(resultsPath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        if (doc.isObject()) {
            const QJsonArray hunks = doc.object().value("hunks").toArray();
            for (const QJsonValue &value : hunks) {
                if (!value.isObject())
                    continue;

                const QJsonObject hunk = value.toObject();
                if (hunk.value("number").toInt() != selectedHunkNumber)
                    continue;

                addClass = hunk.value("additionsClassification")
                               .toString().trimmed();
                delClass = hunk.value("deletionsClassification")
                               .toString().trimmed();
                if (addClass.isEmpty())
                    addClass = "-";
                if (delClass.isEmpty())
                    delClass = "-";
                break;
            }
        }
    }

    context.hunkSideClassifications =
        QString("Hunk_Add_Class: %1 | Hunk_Del_Class: %2")
            .arg(addClass, delClass);
    context.hunkSideClassificationsVisible = true;
    emit stateChanged();
}

void ResultsViewerController::setHunkLabel(const QString &text){
    context.fileClassification = text.isEmpty() ? QStringLiteral("-") : text;
    emit stateChanged();
}

void ResultsViewerController::setHunkLabelFromRepoBase(QString repoBaseAbs)
{
    #ifdef Q_OS_WIN
        repoBaseAbs.replace('\\', '/');
    #endif

    const QStringList parts = repoBaseAbs.split('/', Qt::SkipEmptyParts);

    QString hunk;
    if (parts.size() >= 2){
        hunk = parts.at(parts.size() - 2);
    }

    setHunkLabel(hunk);
}
