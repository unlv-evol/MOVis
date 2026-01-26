#include "headers/similarityInterface.h"
#include "headers/helpers/similarityInterfaceHelpers.h"
#include "headers/helpers/prHelpers.h"
#include "headers/helpers/helpers.h"
#include "headers/helpers/regexHelpers.h"
#include "headers/helpers/fileHelpers.h"
#include "headers/structures/InformedLines.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

similarityInterface::similarityInterface(QWidget *parent, QString rootParam) : QWidget(parent){
    rootDirectory = rootParam;

    QVBoxLayout* rootLayout = new QVBoxLayout(this);

    // 6 pixel padding on all sides
    rootLayout->setContentsMargins(6, 6, 6, 6);
    rootLayout->setSpacing(8); // 8 pixel spacing between child widgets.

    // Create the Top Area that provides the user with context and selection option
    topGroup = new QGroupBox("Context", this);
    classificationLabel = new QLabel("Classification: ", topGroup);
    classificationCombo = new QComboBox(topGroup);
    classificationCombo->setMinimumWidth(160);

    prLabel = new QLabel("PR Number: ", topGroup);
    prCombo = new QComboBox(topGroup);
    prCombo->setMinimumWidth(180);

    QLabel* hunkLabel = NULL;
    buildGroupBoxRows(topGroup, classificationLabel, classificationCombo,
                      prLabel, prCombo, hunkLabel);

    rootLayout->addWidget(topGroup);

    // Create File Comparison Area
    QSplitter* splitWidget = new QSplitter(this);
    rootLayout->addWidget(splitWidget, 1);

    leftPanel = new QWidget(splitWidget);
    QString label = "Left (src): ";
    createSplitWidget(leftPanel, leftFileSelector, leftEdit, label, true);
    splitWidget->addWidget(leftPanel);

    rightPanel = new QWidget(splitWidget);
    label = "Right (cmp): ";
    createSplitWidget(rightPanel, rightFileSelector, rightEdit, label, false);
    splitWidget->addWidget(rightPanel);

    // Make them read only.
    leftEdit->setReadOnly(true);
    rightEdit->setReadOnly(true);

    // Links the QObjects to a slot accordingly.
    connect(classificationCombo,
                     QOverload<int>::of(&QComboBox::currentIndexChanged),
                     this,
                     &similarityInterface::onClassificationChanged);

    connect(prCombo,
                     QOverload<int>::of(&QComboBox::currentIndexChanged),
                     this,
                     &similarityInterface::onPRChanged);

/*    connect(leftFileSelector,
                     QOverload<int>::of(&QComboBox::currentIndexChanged),
                     this,
                     &similarityInterface::switchFilesOnLeft)*/;

    connect(rightFileSelector,
                     QOverload<int>::of(&QComboBox::currentIndexChanged),
                     this,
                     &similarityInterface::switchFilesOnRight);

    connect(leftEdit, &QPlainTextEdit::cursorPositionChanged,
                     this, &similarityInterface::onCursorPositionChanged);

    if(!rootDirectory.isEmpty()){
        scanRootFolderForPRsAndClasses(classificationToPRs, rootDirectory);
    }

    classificationCombo->clear();
    QStringList classes = classificationToPRs.keys();
    classes.sort(Qt::CaseInsensitive);
    classificationCombo->addItems(classes);

    if (classificationCombo->count() > 0) {
        classificationCombo->setCurrentIndex(0);
        onClassificationChanged(0);
    } else {
        prCombo->clear();
        // leftFileSelector->clear();
        rightFileSelector->clear();
    }

    setWindowTitle("MOVis Results Viewer");
    resize(1200, 750);
    splitWidget->setSizes({600, 600});
}

void similarityInterface::setPRHyperlink(QGroupBox *group, const QString &title, const QString &locationUrl){
    if(!group)
        return;

    QLabel* titleLabel = group->findChild<QLabel*>("Title");
    QLabel* urlLabel = group->findChild<QLabel*>("URL");

    if (titleLabel){
        titleLabel->setText(title.isEmpty() ? "-" : title);
    }

    if (urlLabel) {
        if (locationUrl.isEmpty()) {
            urlLabel->setText("-");
        } else {
            const QString safe = locationUrl.toHtmlEscaped();
            // show the URL as clickable text
            urlLabel->setText(QString("<a href=\"%1\">%1</a>").arg(safe));
        }
    }
}

void similarityInterface::initializeHighlights(QVector<HighlightSpec> &specs){
    specs.clear();
}

void similarityInterface::onClassificationChanged(int){
    prCombo->blockSignals(true);
    prCombo->clear();

    const QString classification = classificationCombo->currentText();
    const QSet<QString> prsSet = classificationToPRs.value(classification);
    const QStringList prs = sortedSet(prsSet);

    startingCheck = 0;
    lastCheck = "0";
    lastCheckName = "";
    addedLines = 0;

    prCombo->addItems(prs);
    prCombo->blockSignals(false);

    if (prCombo->count() > 0) {
        prCombo->setCurrentIndex(0);
        onPRChanged(0);
    } else {
        // leftFileSelector->clear();
        rightFileSelector->clear();
        leftEdit->clear();
        leftPath.clear();
        rightEdit->clear();
        rightPath.clear();

        QLabel *hunkValue = topGroup->findChild<QLabel*>("Hunk");

        if (hunkValue)
            hunkValue->setText("-");

        changeGroupBoxValues(topGroup, "-", "-");
        setPRHyperlink(topGroup, "-", "");
    }
}

void similarityInterface::onPRChanged(int){
    const QString classification = classificationCombo->currentText();
    const QString pr  = prCombo->currentText();

    // leftFileSelector->clear();
    leftEdit->clear();
    leftPath.clear();
    QLabel *hunkValue = topGroup->findChild<QLabel*>("HunkValueLabel");
    if (hunkValue) hunkValue->setText("-");
    changeGroupBoxValues(topGroup, "-", "-");

    const QStringList cmpFiles = scallAllFilesInCmp(rootDirectory, pr, classification);
    refillSelectorKeepSelection(rightFileSelector, cmpFiles);

    if (rightFileSelector->currentIndex() >= 0)
        switchFilesOnRight(rightFileSelector->currentIndex());
    else {
        rightEdit->clear();
        rightPath.clear();
    }

}

void similarityInterface::switchFilesOnLeft(int index){
    if (index < 0)
        return;

    const QString logicalSrcPath = leftFileSelector->itemData(index, Qt::UserRole).toString();

    if (logicalSrcPath.isEmpty())
        return;

    const QString actualOpenPath = changeHunkName(logicalSrcPath);

    // if (!loadPathIntoLeftOrRight(actualOpenPath, leftEdit)) {
    //     QMessageBox::warning(this, "Error", "Could not open src file for left editor.");
    //     return;
    // }

    // Keep internal identity as additions/deletions
    leftPath = logicalSrcPath;

    // But remember what we actually opened
    leftOpenPath = actualOpenPath;

    // Clear both panes' extra selections
    leftSelections.clear();
    rightSelections.clear();
    duplicateRegions.clear();
    leftEdit->setExtraSelections(QList<QTextEdit::ExtraSelection>());
    rightEdit->setExtraSelections(QList<QTextEdit::ExtraSelection>());

    // Recompute similarity for the current left/right pair (uses leftPath logical)
    updateSimilarityOnPair();

    initializeHighlights(startupSpecs);
    startupHighlightsRan = false;
    highlightSimilarLines();
}

void similarityInterface::switchFilesOnRight(int index){
    if (index < 0)
        return;

    QString cmpPath = rightFileSelector->itemData(index, Qt::UserRole).toString();

    if (cmpPath.isEmpty())
        return;

    if (!loadPathIntoLeftOrRight(cmpPath, rightEdit)){
        QMessageBox::warning(this, "Error", "Could not open cmp file for right editor.");
        return;
    }

    rightPath = cmpPath;
    rightEdit->clearPerBlockLineNumbers();
    rightEdit->setOverrideLineNumbers(true);
    rightEdit->setLineNumberStart(1);

    currentFile = extractCurrentFileFromCmpPath(cmpPath);

    QString repoBaseAbs;
    const QString cmpString = "cmp";

    if (!obtainRepositoryPath(cmpString, cmpPath, repoBaseAbs)){
        clearLeftPane(true);
        setHunkLabel("-");

        updateSimilarityOnPair();
        initializeHighlights(startupSpecs);
        startupHighlightsRan = false;
        highlightSimilarLines();
        return;
    }

    const QString classification = classificationCombo->currentText();
    const QString pr = prCombo->currentText();
    const QString prclass = buildDirWithPRAndClassification(rootDirectory, pr, classification);

    QStringList srcFiles;
    if (isUnder(prclass, repoBaseAbs)){
        const QString srcString = "src";
        srcFiles = scanFilesUnderPath(srcString, repoBaseAbs);
        sortHunkFiles(srcFiles);
    }

    //refillSelectorKeepSelection(leftFileSelector, srcFiles);
    loadAllLeftHunks(srcFiles);

    // if (!openLeftFromCurrentSelection()){
    //     clearLeftPane(false);
    // }

    setHunkLabelFromRepoBase(repoBaseAbs);

    updateSimilarityOnPair();

    initializeHighlights(startupSpecs);
    startupHighlightsRan = false;
    highlightSimilarLines();
}

void similarityInterface::loadAllLeftHunks(const QStringList& hunkFilesAbs)
{
    leftHunkRanges.clear();

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
    totalFiles = 0;
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

            auto m = reDiffHeader.match(line);
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
        leftHunkIndexByFileName.insert(trueName, hunkIdx);
        leftHunkRanges.push_back(HunkRange{
            startLine,
            endLine,
            trueName,
            absPath
        });
        totalFiles++;
    }
    leftEdit->setPlainText(combined);
    leftEdit->setPerBlockLineNumbers(lines);
}

void similarityInterface::jumpToLine(QPlainTextEdit *editor, int lineOneBased){
    if (lineOneBased < 1)
        lineOneBased = 1;

    QTextDocument *document = editor->document();

    QTextBlock block = document->findBlockByNumber(lineOneBased + 35 - 1);
    if (!block.isValid())
        return;

    QTextCursor cursor(document);
    cursor.setPosition(block.position());
    editor->setTextCursor(cursor);
}

int similarityInterface::toPos(QPlainTextEdit *editor, int lineOneBased, int colOneBased){
    if (lineOneBased < 1)
        lineOneBased = 1;

    QTextDocument *document = editor->document();
    QTextBlock block = document->findBlockByNumber(lineOneBased - 1);
    if (!block.isValid()) return document->characterCount() - 1;

    int basePosition = block.position();
    int zeroBasedColumn = qMax(0, colOneBased - 1);

    return basePosition + zeroBasedColumn;
}

bool similarityInterface::loadPathIntoLeftOrRight(const QString &path, QPlainTextEdit *targetEditor){
    QFile file(path);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    QTextStream textStream(&file);
    textStream.setAutoDetectUnicode(true);
    textStream.setEncoding(QStringConverter::Utf8);


    const QString text = textStream.readAll();
    file.close();

    if (auto *v = qobject_cast<LineNumberHelpers*>(targetEditor)) {
        v->readFile(text);
    } else {
        targetEditor->setPlainText(text);
    }

    return true;
}

void similarityInterface::highlightRange(QPlainTextEdit *editor, int startLine, int startColumn,
                                   int endLine, int endColumn, const QColor &color){

    qDebug()<<"====================================================";
    qDebug()<<"====================================================";
    qDebug()<<"====================================================";

    QTextDocument *document = editor->document();
    int startPos = toPos(editor, startLine, startColumn);
    int endPos = toPos(editor, endLine, endColumn);

    QColor context= QColor(180, 180, 255);
    QColor purple = QColor(255, 180, 255);
    QColor green = QColor(180, 255, 180);
    QColor red = QColor(255, 180, 180);

    if (endPos < startPos) {
        int tmp = startPos;
        startPos = endPos;
        endPos = tmp;
    }

    int localCheck = startingCheck-addedLines;
    if(localCheck < 0)
        localCheck = 0;

    int adding = 0;

    for(QTextBlock startBlock = document->findBlock(startPos);
         startBlock.isValid() && startBlock.position() <= document->findBlock(endPos).position();
         startBlock=startBlock.next()){
        const QString text = startBlock.text();

        if(text.length() == 0){
            startingCheck++;
            localCheck++;
            continue;
        }

        if(editor == leftEdit){
            QTextCursor cursor(document);
            cursor.setPosition(startBlock.position());
            cursor.setPosition(startBlock.position() + startBlock.length(), QTextCursor::KeepAnchor);

            QTextEdit::ExtraSelection selection;
            selection.cursor = cursor;
            qDebug()<<"Text: "<<text;
            if(text[0] != "+" && text[0] != "-"){
                selection.format.setBackground(context);
                if(leftLineCheck.contains(startingCheck) && leftLineCheck[startingCheck]!=context){
                    rightLineCheck.insert(startingCheck, leftLineCheck[startingCheck]);
                }else{
                    leftLineCheck.insert(startingCheck, context);
                    rightLineCheck.insert(startingCheck, context);
                }
                qDebug()<<"***Context***: "<<context;
            }else{
                if(leftLineCheck.contains(startingCheck) && leftLineCheck[startingCheck]!=context){
                    selection.format.setBackground(purple);
                    rightLineCheck.insert(startingCheck, purple);
                    qDebug()<<"***Purple***: "<<purple;
                }

                selection.format.setBackground(color);
                if(!rightLineCheck.contains(startingCheck) ||
                    (leftLineCheck.contains(startingCheck) && leftLineCheck[startingCheck] == context))
                    rightLineCheck.insert(startingCheck, color);

                leftLineCheck.insert(startingCheck, color);
                qDebug()<<"***Color***: "<<color;

            }
            qDebug()<<"Check Number: "<<startingCheck;

            selection.format.setForeground(Qt::black);
            leftSelections.push_back(selection);
            startingCheck++;
            adding++;
        }else{
            QColor existing;
            if(rightLineCheck.contains(localCheck)){
                existing = rightLineCheck[localCheck];
            }else{
                existing = context;
            }
            QTextCursor cursor(document);
            cursor.setPosition(startBlock.position());
            cursor.setPosition(startBlock.position() + startBlock.length(), QTextCursor::KeepAnchor);

            QTextEdit::ExtraSelection selection;
            selection.cursor = cursor;
            selection.format.setBackground(existing);
            // qDebug()<<"Right Color: "<<existing;
            // qDebug()<<"Right Text: "<<text;
            // qDebug()<<"Check Number: "<<localCheck;
            selection.format.setForeground(Qt::black);
            rightSelections.push_back(selection);
            localCheck++;
        }
    }
    addedLines = adding;
}

void similarityInterface::applySelections(QPlainTextEdit *editor){
    if (editor == leftEdit){
        editor->setExtraSelections(leftSelections);
    }
    else{
        editor->setExtraSelections(rightSelections);
    }
}

void similarityInterface::highlightSimilarLines(){
    bool leftHasText = editorHasText(leftEdit);
    bool rightHasText = editorHasText(rightEdit);

    if (!leftHasText && !rightHasText)
        return;

    clearStartupHighlightState();

    int firstRightDupLine = 0;

    if (rightPath.isEmpty()){
        startupHighlightsRan = true;
        return;
    }

    QString repoBaseAbs;
    QString hunkName;
    QString githubPathName;

    if (!getRepoContextFromCmp(repoBaseAbs, hunkName, githubPathName)){
        startupHighlightsRan = true;
        return;
    }

    const QString cls = classificationCombo->currentText();
    const QString pr = prCombo->currentText();
    const QString prRootAbs = buildDirWithPRAndClassification(rootDirectory, pr, cls);

    QFile rf;
    bool usingRepoReport = false;

    if (!openJscpdReport(repoBaseAbs, prRootAbs, rf, usingRepoReport)){
        startupHighlightsRan = true;
        return;
    }

    QJsonArray dups;
    if (!parseDuplicates(rf, dups)){
        rf.close();
        startupHighlightsRan = true;
        return;
    }

    rf.close();

    JscpdMatchKeys keys = buildMatchKeys(repoBaseAbs, hunkName, githubPathName);

    for (int i = 0; i < dups.size(); ++i)
    {
        QJsonValue v = dups.at(i);
        
        if (!v.isObject()){
            continue;
        }

        QJsonObject dup = v.toObject();
        int linesTotal = dup.value("lines").toInt();

        QJsonObject firstObj = dup.value("firstFile").toObject();
        QJsonObject secondObj = dup.value("secondFile").toObject();

        QString firstName = normalizePath(firstObj.value("name").toString());
        QString secondName = normalizePath(secondObj.value("name").toString());

        if (!passesHunkFilter(firstName, secondName, usingRepoReport, hunkName)){
            continue;
        }

        if (!matchesFilePair(firstName, secondName, keys)){
            continue;
        }

        int fStartLine = 0;
        int fStartCol = 0;
        int fEndLine = 0;
        int fEndCol = 0;

        int sStartLine = 0;
        int sStartCol = 0;
        int sEndLine = 0;
        int sEndCol = 0;

        computeRangeFromDup(firstObj, linesTotal, fStartLine, fStartCol, fEndLine, fEndCol, true);
        computeRangeFromDup(secondObj, linesTotal, sStartLine, sStartCol, sEndLine, sEndCol, false);

        QStringList firstObjCheck = firstName.split("/");

        const QString key = firstObjCheck[1];
        
        if (!leftHunkIndexByFileName.contains(key))
            continue;

        int idx = leftHunkIndexByFileName[key];

        int base = leftHunkRanges[idx].startBlock;

        fStartLine = base + (fStartLine);
        fEndLine   = base + (fEndLine);

        QStringList check = key.split("_");

        if(check[1] != lastCheck){
            leftLineCheck.clear();
            rightLineCheck.clear();
            lastCheck = check[1];
            startingCheck = 0;
        }

        if(check[2]!=lastCheckName){
            startingCheck = 0;
            lastCheckName = check[2];
        }

        QColor col;

        if(key.contains("additions")){
            col = QColor(180,255,180);
        }else{
            col = QColor(255,180,180);
        }

        //col = colorSelection(i);

        int leftStartPos = 0;
        int leftEndPos = 0;
        int rightStartPos = 0;
        int rightEndPos = 0;

        if (leftHasText){
            highlightRange(leftEdit, fStartLine, fStartCol, fEndLine, fEndCol+3, col);
            leftStartPos = toPos(leftEdit, fStartLine, fStartCol);
            leftEndPos = toPos(leftEdit, fEndLine, fEndCol);
        }

        if (rightHasText){
            highlightRange(rightEdit, sStartLine, sStartCol, sEndLine, sEndCol, col);
            rightStartPos = toPos(rightEdit, sStartLine, sStartCol);
            rightEndPos = toPos(rightEdit, sEndLine, sEndCol);

            if (firstRightDupLine == 0){
                firstRightDupLine = sStartLine;
            }
        }

        if (leftStartPos >= 0 && rightStartPos >= 0){
            DuplicateRegion reg;
            reg.leftStartPos = leftStartPos;
            reg.leftEndPos = leftEndPos;
            reg.rightStartPos = rightStartPos;
            reg.rightEndPos = rightEndPos;

            duplicateRegions.push_back(reg);
        }
    }

    applySelections(leftEdit);
    applySelections(rightEdit);

    if (firstRightDupLine > 0 && rightEdit != NULL)
        jumpToLine(rightEdit, firstRightDupLine);

    startupHighlightsRan = true;
}

void similarityInterface::onCursorPositionChanged(){
    if (duplicateRegions.isEmpty())
        return;

    QTextCursor c = leftEdit->textCursor();
    int pos = c.position();

    for (int i = 0; i < duplicateRegions.size(); ++i) {
        const DuplicateRegion &reg = duplicateRegions.at(i);
        if (pos >= reg.leftStartPos && pos < reg.leftEndPos) {
            // Jump right editor to the start of this duplicate region
            QTextDocument *doc = rightEdit->document();
            QTextCursor rc(doc);
            rc.setPosition(reg.rightStartPos);
            rightEdit->setTextCursor(rc);
            rightEdit->centerCursor();
            break;
        }
    }
}

bool similarityInterface::setSimilarityResultsFile(const QString &resultsPath, QString *errorOut){
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
    setPRHyperlink(topGroup, similarityParser.getPRTitle(), similarityParser.getPRLocation());
    return true;
}

bool similarityInterface::querySimilarity(const QString &fileName,
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

void similarityInterface::updateSimilarityOnPair(){
    // We need both sides to be loaded to do anything meaningful.
    if (rightPath.isEmpty()) {
        changeGroupBoxValues(topGroup, "-", "-");
        return;
    }

    // Derive the similarity results path from the right cmpPath.
    QStringList testing2 = rightPath.split("Repos_results/");
    if (testing2.size() < 2) {
        changeGroupBoxValues(topGroup, "-", "-");
        return;
    }

    QStringList testing3 = testing2[1].split("/");
    if (testing3.size() < 6) {
        changeGroupBoxValues(topGroup, "-", "-");
        return;
    }

    // Example layout:
    // .../Repos_results/<repo>/<pr>/.../src/hunk_1_additions.scala
    QString simPath = testing2[0]
                      + "Repos_results/"
                      + testing3[0] + "/"
                      + testing3[1] + "/pr_results.txt";

    // Load similarity file only once per PR or when the path changes.
    QString err;
    if (!similarityLoaded || similarityResultsPath != simPath) {
        if (!setSimilarityResultsFile(simPath, &err)) {
            changeGroupBoxValues(topGroup, "-", "-");
            return;
        }
    }
}

// ************************************************************************************
void similarityInterface::resetComparisonState(){
    leftSelections.clear();
    rightSelections.clear();
    duplicateRegions.clear();

    leftEdit->setExtraSelections(QList<QTextEdit::ExtraSelection>());
    rightEdit->setExtraSelections(QList<QTextEdit::ExtraSelection>());
}

void similarityInterface::clearLeftPane(bool clearSelector){
    if (clearSelector){
        // leftFileSelector->clear();
    }

    leftEdit->clear();
    leftPath.clear();
    leftOpenPath.clear();

    resetComparisonState();
}

void similarityInterface::setHunkLabel(const QString &text){
    QLabel *hunkValue = topGroup->findChild<QLabel *>("Hunk");
    if (!hunkValue){
        return;
    }

    if (text.isEmpty()){
        hunkValue->setText("-");
    }
    else{
        hunkValue->setText(text);
    }
}

void similarityInterface::setHunkLabelFromRepoBase(QString repoBaseAbs)
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

bool similarityInterface::openLeftFromCurrentSelection(){
    const int leftIndex = leftFileSelector->currentIndex();
    if (leftIndex < 0){
        return false;
    }

    const QString logicalSrcPath =
        leftFileSelector->itemData(leftIndex, Qt::UserRole).toString();

    if (logicalSrcPath.isEmpty()){
        return false;
    }

    const QString actualOpenPath = changeHunkName(logicalSrcPath);

    if (!loadPathIntoLeftOrRight(actualOpenPath, leftEdit)){
        return false;
    }

    // Keep internal identity as additions/deletions
    leftPath = logicalSrcPath;

    // Track what we actually opened
    leftOpenPath = actualOpenPath;

    resetComparisonState();
    return true;
}

QString similarityInterface::extractCurrentFileFromCmpPath(const QString &cmpPath){
    const QString marker = "Repos_results/";
    const int pos = cmpPath.indexOf(marker);

    if (pos < 0){
        return QString();
    }

    const QString suffix = cmpPath.mid(pos + marker.size());
    const QStringList parts = suffix.split('/', Qt::SkipEmptyParts);

    return parts[5];
}

bool similarityInterface::matchesFilePair(const QString &firstName, const QString &secondName, const JscpdMatchKeys &keys){
    bool leftMatch = false;
    bool rightMatch = false;

    if (firstName.endsWith(keys.leftRelRepo)){
        leftMatch = true;
    }
    else if (firstName.endsWith(keys.leftRelPR)){
        leftMatch = true;
    }
    else if (firstName.endsWith("/" + keys.leftFileName)){
        leftMatch = true;
    }
    else if (firstName == keys.leftFileName){
        leftMatch = true;
    }

    if (secondName.endsWith(keys.rightRelRepo)){
        rightMatch = true;
    }
    else if (secondName.endsWith(keys.rightRelPR)){
        rightMatch = true;
    }
    else if (secondName.endsWith("/" + keys.rightFileName)){
        rightMatch = true;
    }
    else if (secondName == keys.rightFileName){
        rightMatch = true;
    }

    if (!leftMatch || !rightMatch){
        return false;
    }

    return true;
}

bool similarityInterface::passesHunkFilter(const QString &firstName, const QString &secondName, bool usingRepoReport, const QString &hunkName){
    if (usingRepoReport){
        return true;
    }

    bool inHunkFirst = false;
    bool inHunkSecond = false;

    if (firstName.contains("/" + hunkName + "/") || firstName.startsWith(hunkName + "/")){
        inHunkFirst = true;
    }

    if (secondName.contains("/" + hunkName + "/") || secondName.startsWith(hunkName + "/")){
        inHunkSecond = true;
    }

    if (!inHunkFirst || !inHunkSecond){
        return false;
    }

    return true;
}

JscpdMatchKeys similarityInterface::buildMatchKeys(const QString &repoBaseAbs, const QString &hunkName, const QString &githubPathName){
    JscpdMatchKeys keys;
    keys.leftRelRepo = QDir(repoBaseAbs).relativeFilePath(leftPath);
    keys.rightRelRepo = QDir(repoBaseAbs).relativeFilePath(rightPath);

    keys.leftRelRepo = normSlashes(keys.leftRelRepo);
    keys.rightRelRepo = normSlashes(keys.rightRelRepo);

    keys.leftRelRepo = normalizePath(keys.leftRelRepo);
    keys.rightRelRepo = normalizePath(keys.rightRelRepo);

    keys.leftRelPR = hunkName + "/" + githubPathName + "/" + keys.leftRelRepo;
    keys.rightRelPR = hunkName + "/" + githubPathName + "/" + keys.rightRelRepo;

    keys.leftFileName = QFileInfo(leftPath).fileName();
    keys.rightFileName = QFileInfo(rightPath).fileName();

    return keys;
}

bool similarityInterface::parseDuplicates(QFile &rf, QJsonArray &duplicatesOut){
    QByteArray bytes = rf.readAll();

    QJsonParseError perr;
    QJsonDocument doc = QJsonDocument::fromJson(bytes, &perr);

    if (perr.error != QJsonParseError::NoError){
        return false;
    }

    if (!doc.isObject()){
        return false;
    }

    QJsonObject root = doc.object();
    duplicatesOut = root.value("duplicates").toArray();
    return true;
}

bool similarityInterface::openJscpdReport(const QString &repoBaseAbs, const QString &prRootAbs, QFile &rf, bool &usingRepoReport){
    usingRepoReport = false;

    QString repoReportPath = QDir(repoBaseAbs).filePath("reports/html/jscpd-report.json");
    rf.setFileName(repoReportPath);

    if (rf.open(QIODevice::ReadOnly)){
        usingRepoReport = true;
        return true;
    }

    QString prReportPath = QDir(prRootAbs).filePath("reports/html/jscpd-report.json");
    rf.setFileName(prReportPath);

    if (rf.open(QIODevice::ReadOnly)){
        usingRepoReport = false;
        return true;
    }

    return false;
}

bool similarityInterface::getRepoContextFromCmp(QString &repoBaseAbs, QString &hunkName, QString &githubPathName){
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

void similarityInterface::clearStartupHighlightState(){
    leftSelections.clear();
    rightSelections.clear();
    duplicateRegions.clear();

    applySelections(leftEdit);
    applySelections(rightEdit);
}
