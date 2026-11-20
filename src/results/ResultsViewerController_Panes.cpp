// Results viewer: filling the two code panes.
//
// The right pane shows one comparison file. The left pane shows the generated
// hunk files for the same pull request, concatenated into a single document
// with the original line numbers restored, so both sides can be read against
// each other. The hunk selector narrows the left pane to one hunk.

#include "headers/results/ResultsViewerController.h"
#include "headers/helpers/fileHelpers.h"
#include "headers/helpers/helpers.h"
#include "headers/helpers/prHelpers.h"
#include "headers/helpers/regexHelpers.h"
#include "headers/results/ResultFiles.h"
#include "headers/structures/InformedLines.h"
#include <QClipboard>
#include <QDebug>
#include <QGuiApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QTextBlock>
#include <QTextStream>
#include <QUrl>
#include <algorithm>

void ResultsViewerController::onHunkViewChanged(int index){
    applyHunkSelection(index);

    // However that turned out, both panes start again from their first line
    // with freshly read documents.
    emit stateChanged();
    emit panesReset();
}

void ResultsViewerController::applyHunkSelection(int index){
    if (!leftFileSelector || index < 0)
        return;

    clearSearchFields();

    selectedHunkNumber =
        leftFileSelector->valueAt(index).toInt();

    if (currentLeftHunkFiles.isEmpty()) {
        leftEdit->clear();
        leftHunkRanges.clear();
        leftHunkIndexByFileName.clear();
        return;
    }

    // Rebuild only the left presentation.  Selecting "Hunk N" keeps both
    // additions and deletions for N; it never changes classification data or
    // the cmp file on the right.
    loadAllLeftHunks(currentLeftHunkFiles);

    resetComparisonState();
    updateSimilarityOnPair();
    highlightSimilarLines();
    updateSelectedHunkClassificationDisplay();
}

void ResultsViewerController::switchFilesOnRight(int index){
    applyRightFileSelection(index);

    emit stateChanged();
    emit panesReset();
}

void ResultsViewerController::applyRightFileSelection(int index){
    if (index < 0)
        return;

    clearSearchFields();

    const QString selectedPath =
        rightFileSelector->valueAt(index).toString();

    if (selectedPath.isEmpty())
        return;

    const QString classification = classificationCombo->currentText();
    const QString pr = prCombo->currentText();

    const QString individualResultsPath = individualResultsForSelectedPath(
        rootDirectory, pr, classification, selectedPath);
    setFileResultsLink(individualResultsPath);

    // CC, NE, and ERROR do not require a comparison workspace. Their selector contains
    // the individual results.json files directly so users can still inspect
    // every terminally classified file and open its complete result record.
    if (isTerminalVisualizerClassification(classification) &&
        QFileInfo(selectedPath).fileName().compare(
            "results.json", Qt::CaseInsensitive) == 0) {
        if (!loadPathIntoLeftOrRight(selectedPath, rightEdit)) {
            emit messageRequested(
                "Error", "Could not open the selected file results.json.");
            return;
        }

        rightPath = selectedPath;

        rightEdit->clearPerBlockLineNumbers();
        rightEdit->setOverrideLineNumbers(true);
        rightEdit->setLineNumberStart(1);

        currentLeftHunkFiles.clear();
        populateHunkViewSelector(currentLeftHunkFiles);
        clearLeftPane(true);
        setHunkLabel(classification);
        if (classification == "ERROR") {
            setErrorDetails(individualResultErrorSummary(selectedPath));
        } else {
            setErrorDetails(QString());
        }
        return;
    }

    setErrorDetails(QString());
    const QString cmpPath = selectedPath;

    if (!loadPathIntoLeftOrRight(cmpPath, rightEdit)){
        emit messageRequested("Error", "Could not open cmp file for right editor.");
        return;
    }

    rightPath = cmpPath;
    rightEdit->clearPerBlockLineNumbers();
    rightEdit->setOverrideLineNumbers(true);
    rightEdit->setLineNumberStart(1);

    QString repoBaseAbs;
    const QString cmpString = "cmp";

    if (!obtainRepositoryPath(cmpString, rightPath, repoBaseAbs)){
        currentLeftHunkFiles.clear();
        populateHunkViewSelector(currentLeftHunkFiles);
        clearLeftPane(true);
        setHunkLabel("-");

        updateSimilarityOnPair();
        highlightSimilarLines();
        return;
    }

    const QString prclass =
        buildDirWithPRAndClassification(rootDirectory, pr, classification);

    QStringList srcFiles;
    if (isUnder(prclass, repoBaseAbs)){
        const QString srcString = "src";
        srcFiles = scanFilesUnderPath(srcString, repoBaseAbs);
        sortHunkFiles(srcFiles);
    }

    currentLeftHunkFiles = srcFiles;
    populateHunkViewSelector(currentLeftHunkFiles);
    loadAllLeftHunks(currentLeftHunkFiles);

    setHunkLabelFromRepoBase(repoBaseAbs);

    updateSimilarityOnPair();

    highlightSimilarLines();
}

void ResultsViewerController::populateHunkViewSelector(
    const QStringList &hunkFilesAbs)
{
    if (!leftFileSelector)
        return;

    static const QRegularExpression logicalHunkPattern(
        R"(^hunk_(\d+)_(additions|deletions)(?:\.[^/\\]+)?$)",
        QRegularExpression::CaseInsensitiveOption
        );

    QSet<int> hunkNumbers;
    for (const QString &path : hunkFilesAbs) {
        const QString fileName = QFileInfo(path).fileName();
        const QRegularExpressionMatch match =
            logicalHunkPattern.match(fileName);
        if (match.hasMatch())
            hunkNumbers.insert(match.captured(1).toInt());
    }

    QList<int> sortedHunks = hunkNumbers.values();
    std::sort(sortedHunks.begin(), sortedHunks.end());

    leftFileSelector->blockSignals(true);
    leftFileSelector->clear();
    leftFileSelector->addItem("ALL HUNKS SHOWING", 0);
    for (const int hunkNumber : sortedHunks) {
        leftFileSelector->addItem(
            QString("Hunk %1").arg(hunkNumber), hunkNumber);
    }
    selectedHunkNumber = 0;
    updateSelectedHunkClassificationDisplay();
    leftFileSelector->setCurrentIndex(0);
    leftFileSelector->setEnabled(!sortedHunks.isEmpty());
    leftFileSelector->blockSignals(false);
}

void ResultsViewerController::loadAllLeftHunks(const QStringList& hunkFilesAbs)
{
    leftHunkRanges.clear();
    leftHunkIndexByFileName.clear();

    QString combined;
    QVector<int> lines;

    int currentLine = 0; // QTextBlock index (0-based)

    // Regex for files to hide
    QRegularExpression reContext("^hunk_\\d+_context(\\.[^/\\\\]+)?$");
    QRegularExpression reAddition("^hunk_\\d+_additions(\\.[^/\\\\]+)?$");
    QRegularExpression reDeletion("^hunk_\\d+_deletions(\\.[^/\\\\]+)?$");
    QRegularExpression reDiffHeader(
        R"(^\s*@@\s+-\d+(?:,\d+)?\s+\+(\d+)(?:,\d+)?\s+@@)"
    );
    // The combined left pane displays the full_add/full_del files, but labels
    // them using their logical additions/deletions names.  Only expose a side
    // when that logical hunk file was actually generated.  Otherwise a lone
    // full_add/full_del would make the UI invent a hunk_N_additions or
    // hunk_N_deletions entry that does not exist in src/.
    QSet<QString> existingLogicalHunks;
    for (const QString& absPath : hunkFilesAbs) {
        const QString fileName = QFileInfo(absPath).fileName();
        if (reAddition.match(fileName).hasMatch() ||
            reDeletion.match(fileName).hasMatch()) {
            existingLogicalHunks.insert(fileName.toCaseFolded());
        }
    }

    for (const QString& absPath : hunkFilesAbs)
    {
        QFileInfo fileInfo(absPath);
        QFile file(absPath);

        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qDebug() << "FAILED OPEN:" << absPath << "error:" << file.errorString();
            continue;
        }

        const QString fileName = fileInfo.fileName();

        if(fileName.contains(reContext) || fileName.contains(reAddition) || fileName.contains(reDeletion)){
            continue;
        }

        QStringList noExtension = fileInfo.fileName().split(".");
        QStringList trueNameList = noExtension[0].split("_");
        QString trueName = trueNameList[0]+"_"+trueNameList[1];

        if(trueNameList[3] == "add"){
            trueName += "_additions." +noExtension[1];
        }else{
            trueName += "_deletions." +noExtension[1];
        }

        // Hunk filtering is by numeric hunk identity, not by side.  Therefore
        // selecting Hunk 6 keeps both hunk_6_additions and hunk_6_deletions
        // while excluding every other hunk.  The trailing underscore avoids
        // the old hunk_1 / hunk_10 prefix ambiguity.
        if (selectedHunkNumber > 0 &&
            !trueName.toCaseFolded().startsWith(
                QString("hunk_%1_").arg(selectedHunkNumber))) {
            continue;
        }

        // full_add/full_del are display backing files only.  If the matching
        // additions/deletions file is absent, that side does not exist for this
        // hunk and must not be shown.
        if (!existingLogicalHunks.contains(trueName.toCaseFolded())) {
            continue;
        }

        int hunkLineCounter = -1;
        const QString header = QString("===== %1 =====\n")
                                   .arg(trueName);

        // header occupies 1 block (line)
        combined += header;
        lines.push_back(0);
        currentLine += 1;

        int startLine = currentLine;

        // read lines and append
        QTextStream in(&file);
        while (!in.atEnd()) {
            const QString line = in.readLine();
            combined += line;
            combined += "\n";
            currentLine += 1;

            QRegularExpressionMatch m = reDiffHeader.match(line);
            if (m.hasMatch()) {
                // don't number the @@ line itself
                hunkLineCounter = m.captured(1).toInt()-1;
                lines.push_back(0);
                continue;
            }

            if (hunkLineCounter > 0) {
                lines.push_back(hunkLineCounter);
                hunkLineCounter += 1;
            } else {
                lines.push_back(0);
            }
        }

        // add spacer line between hunks
        combined += "\n";
        lines.push_back(0);
        currentLine += 1;

        int endLine = currentLine;
        const int hunkIdx = leftHunkRanges.size();
        leftHunkIndexByFileName.insert(trueName.toCaseFolded(), hunkIdx);
        leftHunkRanges.push_back(HunkRange{
            startLine,
            endLine,
            trueName,
            absPath
        });
    }
    leftEdit->setPlainText(combined);
    leftEdit->setPerBlockLineNumbers(lines);
}

void ResultsViewerController::jumpToLine(CodeDocument *editor, int lineOneBased){
    if (lineOneBased < 1)
        lineOneBased = 1;

    QTextDocument *document = editor->document();

    QTextBlock block = document->findBlockByNumber(lineOneBased - 1);
    if (!block.isValid())
        return;

    QTextCursor cursor(document);
    cursor.setPosition(block.position());
    editor->setTextCursor(cursor);
}

int ResultsViewerController::toPos(CodeDocument *editor, int lineOneBased, int colOneBased){
    if (lineOneBased < 1)
        lineOneBased = 1;

    QTextDocument *document = editor->document();
    QTextBlock block = document->findBlockByNumber(lineOneBased - 1);
    if (!block.isValid()) return document->characterCount() - 1;

    const int basePosition = block.position();
    const int maxColumn = qMax(0, block.length() - 1);
    const int zeroBasedColumn = qBound(0, colOneBased - 1, maxColumn);

    return basePosition + zeroBasedColumn;
}

bool ResultsViewerController::loadPathIntoLeftOrRight(const QString &path, CodeDocument *targetEditor){
    QFile file(path);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    QTextStream textStream(&file);
    textStream.setAutoDetectUnicode(true);
    textStream.setEncoding(QStringConverter::Utf8);


    const QString text = textStream.readAll();
    file.close();

    targetEditor->setPlainText(text);

    return true;
}

void ResultsViewerController::clearLeftPane(bool clearSelector){
    if (clearSelector && leftFileSelector){
        leftFileSelector->blockSignals(true);
        leftFileSelector->clear();
        leftFileSelector->addItem("ALL HUNKS SHOWING", 0);
        leftFileSelector->setCurrentIndex(0);
        leftFileSelector->setEnabled(false);
        leftFileSelector->blockSignals(false);
        selectedHunkNumber = 0;
        updateSelectedHunkClassificationDisplay();
    }

    leftEdit->clear();
    leftPath.clear();

    resetComparisonState();
}

void ResultsViewerController::clearSearchFields(){
    // The search fields themselves live in QML; this just tells them to reset.
    emit searchFieldsCleared();
}

bool ResultsViewerController::getRepoContextFromCmp(QString &repoBaseAbs, QString &hunkName, QString &githubPathName){
    if (rightPath.isEmpty())
        return false;

    const QString cmpString = "cmp";
    if (!obtainRepositoryPath(cmpString, rightPath, repoBaseAbs))
        return false;

    QString tmp = normSlashes(repoBaseAbs);
    const QStringList parts = tmp.split('/', Qt::SkipEmptyParts);

    if (parts.size() >= 2){
        hunkName = parts.at(parts.size() - 2);
    }
    else{
        hunkName.clear();
    }

    if (parts.size() >= 1){
        githubPathName = parts.at(parts.size() - 1);
    }
    else{
        githubPathName.clear();
    }

    return true;
}

QString ResultsViewerController::normalizedSearchText(
    const QString &selectedText,
    const QString &typedText) const
{
    QString text = selectedText.trimmed().isEmpty()
        ? typedText
        : selectedText;

    if (text.trimmed().isEmpty()) {
        const QClipboard* clipboard = QGuiApplication::clipboard();
        if (clipboard) {
            text = clipboard->text();
        }
    }

    // QTextCursor::selectedText() uses paragraph separators instead of normal newlines.
    text.replace(QChar::ParagraphSeparator, '\n');
    text = text.trimmed();

    // For multi-line selections, searching the first meaningful line is usually more reliable.
    // This helps when copying code blocks or hunks.
    const QStringList lines = text.split('\n', Qt::SkipEmptyParts);
    for (const QString& line : lines) {
        const QString cleaned = line.trimmed();
        if (!cleaned.isEmpty()) {
            return cleaned;
        }
    }

    return text;
}

bool ResultsViewerController::findTextInEditor(CodeDocument *editor,
                                           const QString &text)
{
    if (!editor || text.trimmed().isEmpty()) {
        return false;
    }

    QTextDocument* doc = editor->document();
    if (!doc) {
        return false;
    }

    QString needle = text.trimmed();

    QTextCursor startCursor = editor->textCursor();
    QTextCursor found = doc->find(needle, startCursor);

    // Wrap around if not found after current cursor position.
    if (found.isNull()) {
        QTextCursor beginning(doc);
        beginning.movePosition(QTextCursor::Start);
        found = doc->find(needle, beginning);
    }

    if (found.isNull()) {
        emit messageRequested("Search",
                              "Text was not found on the other side.");
        return false;
    }

    editor->setTextCursor(found);
    return true;
}

void ResultsViewerController::findLeftTextInRight(const QString &selectedText,
                                              const QString &searchText)
{
    findTextInEditor(rightEdit, normalizedSearchText(selectedText, searchText));
}

void ResultsViewerController::findRightTextInLeft(const QString &selectedText,
                                              const QString &searchText)
{
    findTextInEditor(leftEdit, normalizedSearchText(selectedText, searchText));
}

void ResultsViewerController::leftCursorMoved(int position)
{
    leftEdit->moveCursorTo(position);
}

QUrl ResultsViewerController::localFileUrl(const QString &path) const
{
    return QUrl::fromLocalFile(path);
}
