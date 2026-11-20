// Results viewer: similarity highlighting and jumping between the panes.
//
// The colours come from a jscpd report produced during classification. Each
// duplicate it lists is matched back to a line in the left pane and a line in
// the right one, which drives two things: the line tint, and the region table
// that makes clicking a highlighted line on the left scroll the right pane to
// its counterpart.

#include "headers/results/ResultsViewerController.h"
#include "headers/GACPD/Methods/appPath.h"
#include "headers/helpers/fileHelpers.h"
#include "headers/helpers/helpers.h"
#include "headers/helpers/prHelpers.h"
#include "headers/results/HunkPaths.h"
#include "headers/results/LegacyReports.h"
#include "headers/results/ResultFiles.h"
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QTextBlock>
#include <algorithm>
#include <functional>

void ResultsViewerController::highlightRange(CodeDocument *editor, int startLine, int startColumn,
                                   int endLine, int endColumn, const QColor &color){
    Q_UNUSED(startColumn);
    Q_UNUSED(endColumn);

    if (!editor || startLine < 1 || endLine < 1) {
        return;
    }

    if (endLine < startLine) {
        qSwap(startLine, endLine);
    }

    QTextDocument *document = editor->document();
    QTextBlock block = document->findBlockByNumber(startLine - 1);
    const QTextBlock endBlock = document->findBlockByNumber(endLine - 1);

    if (!block.isValid() || !endBlock.isValid()) {
        return;
    }

    QVector<TextExtraSelection> &selections =
        (editor == leftEdit) ? leftSelections : rightSelections;

    while (block.isValid() && block.blockNumber() <= endBlock.blockNumber()) {
        QTextCursor cursor(block);
        cursor.select(QTextCursor::LineUnderCursor);

        TextExtraSelection selection;
        selection.cursor = cursor;
        selection.format.setBackground(color);
        selection.format.setForeground(Qt::black);
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selections.push_back(selection);

        block = block.next();
    }
}

void ResultsViewerController::applySelections(CodeDocument *editor){
    if (editor == leftEdit){
        editor->setExtraSelections(leftSelections);
    }
    else{
        editor->setExtraSelections(rightSelections);
    }
}

// ************************************************************************************
void ResultsViewerController::resetComparisonState(){
    leftSelections.clear();
    rightSelections.clear();
    duplicateRegions.clear();

    leftEdit->setExtraSelections({});
    rightEdit->setExtraSelections({});
}

static void addEvidence(QHash<int, int> &evidence, int line, int flag){
    if (line > 0) {
        evidence[line] = evidence.value(line, 0) | flag;
    }
}

static QString lineText(CodeDocument *editor, int oneBasedLine){
    if (!editor || oneBasedLine < 1) {
        return QString();
    }

    const QTextBlock block =
        editor->document()->findBlockByNumber(oneBasedLine - 1);
    return block.isValid() ? block.text() : QString();
}

void ResultsViewerController::highlightSimilarLines(){
    const bool leftHasText = leftEdit && leftEdit->document()
        && leftEdit->document()->characterCount() > 1;
    const bool rightHasText = rightEdit && rightEdit->document()
        && rightEdit->document()->characterCount() > 1;

    if (!leftHasText && !rightHasText)
        return;



    resetComparisonState();

    int firstRightDupLine = 0;

    if (rightPath.isEmpty())
    {
        return;
    }

    QString repoBaseAbs;
    QString hunkName;
    QString githubPathName;

    if (!getRepoContextFromCmp(repoBaseAbs, hunkName, githubPathName))
    {
        return;
    }

    const QString cls = classificationCombo->currentText();
    const QString pr = prCombo->currentText();
    const QString prRootAbs = buildDirWithPRAndClassification(rootDirectory, pr, cls);

    QFile rf;
    bool usingRepoReport = false;

    if (!openJscpdReport(repoBaseAbs, prRootAbs, rf, usingRepoReport))
    {
        return;
    }

    QJsonArray dups;
    if (!parseDuplicates(rf, dups)){
        rf.close();
        return;
    }

    rf.close();

    // New GACPD runs preserve the dedicated 10-token context scan separately
    // because later classification passes overwrite jscpd-report.json. Never
    // fall back to the normal additions/deletions report as context evidence.
    // For old result folders we either rebuild an isolated context-only report
    // below or leave contextDups empty.
    QJsonArray contextDups;
    bool contextUsingRepoReport = usingRepoReport;
    QFile contextRf;
    bool loadedDedicatedContextReport = false;

    if (openJscpdContextReport(repoBaseAbs, prRootAbs,
                               contextRf, contextUsingRepoReport)) {
        QJsonArray preservedContextDups;
        if (parseDuplicates(contextRf, preservedContextDups)) {
            contextDups = preservedContextDups;
            loadedDedicatedContextReport = true;
        }
        contextRf.close();
    }

    if (!loadedDedicatedContextReport) {
        QJsonArray rebuiltContextDups;
        if (rebuildContextDuplicatesForLegacyResult(
                repoBaseAbs, rightPath, rebuiltContextDups)) {
            contextDups = rebuiltContextDups;
            contextUsingRepoReport = true;
        }
    }

    JscpdMatchKeys keys = buildMatchKeys(repoBaseAbs, hunkName, githubPathName);

    // Rebuild the same per-hunk direct-clone presence used by GACPD's
    // classifier. Context ownership depends on whether a direct additions or
    // deletions check matched at all; clone size must not change ownership.
    const QHash<QString, bool> classificationClonePresenceByHunk =
        classificationClonePresenceByAddDeleteHunk(
            dups, repoBaseAbs, keys, usingRepoReport);

    // The saved similarity audit spans every threshold that each hunk actually
    // reached. It is therefore the authoritative way to know whether a hunk
    // classified normally at 50/40/30/20/(10). The final jscpd-report.json is
    // only the last JSCPD run and can otherwise make a previously resolved hunk
    // look unresolved when another hunk continued to a lower threshold.
    const QString individualResultsPath = individualResultsForSelectedPath(
        rootDirectory, pr, cls, rightPath);
    bool hasDirectCloneAuditMetadata = false;
    const QSet<QString> normalDirectCloneHunks =
        directCloneHunksFromIndividualResult(
            individualResultsPath, hasDirectCloneAuditMetadata);

    // Build line-level evidence first, then color once at the end.  The
    // generated additions/deletions files contain unchanged context lines in
    // addition to the real '+'/'-' patch lines, so coloring a whole JSCPD
    // range green/red makes unchanged code look patched.
    enum HighlightEvidence {
        AdditionEvidence        = 0x01,
        DeletionEvidence        = 0x02,
        ContextEvidence         = 0x04, // fallback context used by classification
        BothEvidence            = 0x08,
        SupportingContextEvidence = 0x10 // display-only context for transparency
    };

    QHash<int, int> leftEvidenceByLine;
    QHash<int, int> rightEvidenceByLine;

    for (int i = 0; i < dups.size(); ++i)
    {
        QJsonValue v = dups.at(i);
        if (!v.isObject())
        {
            continue;
        }

        QJsonObject dup = v.toObject();
        int linesTotal = dup.value("lines").toInt();

        QJsonObject firstObj = dup.value("firstFile").toObject();
        QJsonObject secondObj = dup.value("secondFile").toObject();

        QString firstName = normalizePath(firstObj.value("name").toString());
        QString secondName = normalizePath(secondObj.value("name").toString());

        if (!passesHunkFilter(firstName, secondName, usingRepoReport, hunkName))
        {
            continue;
        }

        if (!matchesFilePair(firstName, secondName, keys))
        {
            continue;
        }

        // jscpd v4 commonly emitted the generated hunk as firstFile, while
        // jscpd v5 commonly emits the repository file first. Normalize that
        // ordering before calculating ranges or looking up the left hunk.
        const QString firstHunkKey = displayedHunkKey(firstName);
        const QString secondHunkKey = displayedHunkKey(secondName);

        const bool firstIsLeft = !firstHunkKey.isEmpty() &&
                                 leftHunkIndexByFileName.contains(firstHunkKey);
        const bool firstIsRight = reportedPathMatchesRight(firstName, keys);
        const bool secondIsLeft = !secondHunkKey.isEmpty() &&
                                  leftHunkIndexByFileName.contains(secondHunkKey);
        const bool secondIsRight = reportedPathMatchesRight(secondName, keys);

        QJsonObject hunkObj;
        QJsonObject cmpObj;
        QString hunkReportedName;
        QString hunkDisplayKey;

        if (firstIsLeft && secondIsRight) {
            hunkObj = firstObj;
            cmpObj = secondObj;
            hunkReportedName = firstName;
            hunkDisplayKey = firstHunkKey;
        } else if (secondIsLeft && firstIsRight) {
            hunkObj = secondObj;
            cmpObj = firstObj;
            hunkReportedName = secondName;
            hunkDisplayKey = secondHunkKey;
        } else {
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

        computeRangeFromDup(hunkObj, linesTotal,
                            fStartLine, fStartCol, fEndLine, fEndCol,
                            displayedHunkLineOffset(hunkReportedName));
        computeRangeFromDup(cmpObj, linesTotal,
                            sStartLine, sStartCol, sEndLine, sEndCol, 0);

        if (!leftHunkIndexByFileName.contains(hunkDisplayKey)) {
            continue;
        }

        const int idx = leftHunkIndexByFileName.value(hunkDisplayKey);
        const int base = leftHunkRanges.at(idx).startBlock;

        // startBlock is the zero-based block containing the displayed @@ line.
        // The range values are one-based, so adding the block index maps them
        // directly into the combined left editor.
        fStartLine += base;
        fEndLine += base;

        const bool additionsSide =
            hunkDisplayKey.contains("_additions");
        const bool deletionsSide =
            hunkDisplayKey.contains("_deletions");
        int leftStartPos = 0;
        int leftEndPos = 0;
        int rightStartPos = 0;
        int rightEndPos = 0;

        if (leftHasText)
        {
            for (int line = fStartLine; line <= fEndLine; ++line) {
                const QString text = lineText(leftEdit, line);

                const bool actualAddition =
                    additionsSide &&
                    text.startsWith("+") && !text.startsWith("+++");
                const bool actualDeletion =
                    deletionsSide &&
                    text.startsWith("-") && !text.startsWith("---");

                if (actualAddition) {
                    addEvidence(leftEvidenceByLine, line, AdditionEvidence);
                } else if (actualDeletion) {
                    addEvidence(leftEvidenceByLine, line, DeletionEvidence);
                }
            }

            leftStartPos = toPos(leftEdit, fStartLine, fStartCol);
            leftEndPos = toPos(leftEdit, fEndLine, fEndCol);
        }

        if (rightHasText)
        {
            for (int line = sStartLine; line <= sEndLine; ++line) {
                // The cmp side has no '+'/'-' prefix of its own.  Classify
                // each matched cmp line from the corresponding displayed
                // patch line instead of coloring the entire duplicate range
                // according to the additions/deletions file that JSCPD used.
                // Unchanged lines stay uncolored here; only the dedicated
                // context fallback pass is allowed to create blue evidence.
                const int relativeLine = line - sStartLine;
                const int matchingLeftLine = fStartLine + relativeLine;

                if (matchingLeftLine >= fStartLine &&
                    matchingLeftLine <= fEndLine) {
                    const QString matchingText =
                        lineText(leftEdit, matchingLeftLine);

                    const bool actualAddition =
                        additionsSide &&
                        matchingText.startsWith("+") &&
                        !matchingText.startsWith("+++");
                    const bool actualDeletion =
                        deletionsSide &&
                        matchingText.startsWith("-") &&
                        !matchingText.startsWith("---");

                    if (actualAddition) {
                        addEvidence(rightEvidenceByLine, line,
                                    AdditionEvidence);
                    } else if (actualDeletion) {
                        addEvidence(rightEvidenceByLine, line,
                                    DeletionEvidence);
                    }
                }
            }

            rightStartPos = toPos(rightEdit, sStartLine, sStartCol);
            rightEndPos = toPos(rightEdit, sEndLine, sEndCol);

            if (firstRightDupLine == 0)
            {
                firstRightDupLine = sStartLine;
            }
        }

        if (leftStartPos >= 0 && rightStartPos >= 0)
        {
            DuplicateRegion reg;
            reg.leftStartPos = leftStartPos;
            reg.leftEndPos = leftEndPos;
            reg.rightStartPos = rightStartPos;
            reg.rightEndPos = rightEndPos;

            duplicateRegions.push_back(reg);
        }
    }

    // Context fallback visualization.
    //
    // hunk_N_context is a single shared context stream for the whole hunk.
    // It must never be assigned independently to additions or deletions. If a
    // hunk reached a normal MO/ED result at any threshold, context is ignored.
    // Otherwise the same matched context lines are mapped onto every displayed
    // projection of that hunk (additions and/or deletions).
    const std::function<QMap<int, int>(const QString &, int, int)> mapContextRangeToCombinedLeft =
        [this](const QString &displayKey,
               int contextStartLine,
               int contextEndLine) -> QMap<int, int> {
        QMap<int, int> mappedLinesByContextLine;

        if (contextStartLine < 1 || contextEndLine < contextStartLine ||
            !leftHunkIndexByFileName.contains(displayKey)) {
            return mappedLinesByContextLine;
        }

        const int idx = leftHunkIndexByFileName.value(displayKey);
        if (idx < 0 || idx >= leftHunkRanges.size()) {
            return mappedLinesByContextLine;
        }

        const HunkRange &range = leftHunkRanges.at(idx);
        QFile displayedFile(range.filePath);

        if (!displayedFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return mappedLinesByContextLine;
        }

        const bool additionsSide = displayKey.contains("_additions");
        const bool deletionsSide = displayKey.contains("_deletions");
        if (!additionsSide && !deletionsSide) {
            return mappedLinesByContextLine;
        }

        QTextStream in(&displayedFile);
        int displayedLine = 0;
        int contextLine = 0;

        while (!in.atEnd()) {
            const QString line = in.readLine();
            ++displayedLine;

            // The @@ header is not present in hunk_N_context.
            if (line.startsWith("@@")) {
                continue;
            }

            // full_add/full_del contain the real changed lines; the dedicated
            // context file omits them. Skipping only the actual side-specific
            // changes reconstructs the exact shared hunk_N_context line index.
            if (additionsSide &&
                line.startsWith("+") && !line.startsWith("+++")) {
                continue;
            }

            if (deletionsSide &&
                line.startsWith("-") && !line.startsWith("---")) {
                continue;
            }

            ++contextLine;

            if (contextLine >= contextStartLine &&
                contextLine <= contextEndLine) {
                mappedLinesByContextLine.insert(
                    contextLine,
                    range.startBlock + displayedLine);
            }

            if (contextLine > contextEndLine) {
                break;
            }
        }

        return mappedLinesByContextLine;
    };

    // Map ONLY the dedicated JSCPD-detected context range to the cmp editor.
    //
    // The context file can contain more unchanged lines than the clone that
    // JSCPD actually reported. Those surrounding lines are useful as source
    // material for alignment, but they are NOT evidence and must never be
    // highlighted merely because they belong to the same hunk.
    //
    // rightStartLine/rightEndLine are the exact cmp-side anchors reported by
    // JSCPD. Intermediate detected context lines are matched monotonically
    // between those anchors so formatting-only differences and repeated lines
    // do not cause us to paint unrelated context.
    const std::function<QMap<int, int>(const QString &, const QString &,
                                       int, int, int, int, int)> mapContextRangeToRight =
        [this, &rightHasText](const QString &reportedContextName,
                             const QString &repoBase,
                             int contextStartLine,
                             int contextEndLine,
                             int rightStartLine,
                             int rightEndLine,
                             int extraSearchLines) -> QMap<int, int> {
        Q_UNUSED(extraSearchLines);

        QMap<int, int> mappedRightByContextLine;

        if (!rightHasText || contextStartLine < 1 ||
            contextEndLine < contextStartLine || rightStartLine < 1 ||
            rightEndLine < rightStartLine) {
            return mappedRightByContextLine;
        }

        const QString contextBaseName =
            QFileInfo(normalizePath(reportedContextName)).fileName();
        const QString contextPath =
            QDir(QDir(repoBase).filePath("src")).filePath(contextBaseName);

        QFile contextFile(contextPath);
        if (!contextFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return mappedRightByContextLine;
        }

        struct ContextSample {
            int sourceLine = 0;
            QString text;
            QString normalizedText;
        };

        QVector<ContextSample> contextSamples;
        QTextStream contextIn(&contextFile);
        int sourceLine = 0;
        while (!contextIn.atEnd()) {
            QString text = contextIn.readLine();
            ++sourceLine;

            // Unified-diff context lines have one marker space that is not
            // present in the real source file.
            if (text.startsWith(' ')) {
                text.remove(0, 1);
            }

            ContextSample sample;
            sample.sourceLine = sourceLine;
            sample.text = text;
            sample.normalizedText = text.simplified();
            contextSamples.push_back(sample);
        }

        const int contextSampleCount =
            static_cast<int>(contextSamples.size());
        if (contextSampleCount == 0 ||
            contextStartLine > contextSampleCount) {
            return mappedRightByContextLine;
        }

        const int detectedStartIndex = contextStartLine - 1;
        const int detectedEndIndex = qMin(
            contextEndLine - 1,
            contextSampleCount - 1);
        if (detectedEndIndex < detectedStartIndex) {
            return mappedRightByContextLine;
        }

        const int maxRightLine = rightEdit->document()->blockCount();
        if (rightStartLine > maxRightLine) {
            return mappedRightByContextLine;
        }
        rightEndLine = qMin(rightEndLine, maxRightLine);
        if (rightEndLine < rightStartLine) {
            return mappedRightByContextLine;
        }

        const std::function<QString(int)> rightLineText = [this](int oneBasedLine) -> QString {
            const QTextBlock block =
                rightEdit->document()->findBlockByNumber(oneBasedLine - 1);
            return block.isValid() ? block.text() : QString();
        };

        const std::function<bool(const ContextSample &, int)> sampleMatchesRight =
            [&rightLineText](const ContextSample &sample,
                             int rightLine) -> bool {
            const QString rightText = rightLineText(rightLine);
            if (sample.text == rightText) {
                return true;
            }

            // Permit formatting-only differences for nonblank lines. Blank
            // lines require exact equality so they cannot jump to an unrelated
            // blank line inside the clone span.
            return !sample.normalizedText.isEmpty() &&
                   sample.normalizedText == rightText.simplified();
        };

        const int detectedCount =
            detectedEndIndex - detectedStartIndex + 1;
        const int rightSpanCount = rightEndLine - rightStartLine + 1;

        // JSCPD gives exact endpoint locations. The first/last physical lines
        // are therefore safe anchors even if the clone starts/ends mid-line.
        mappedRightByContextLine.insert(
            contextSamples.at(detectedStartIndex).sourceLine,
            rightStartLine);

        if (detectedCount == 1) {
            return mappedRightByContextLine;
        }

        mappedRightByContextLine.insert(
            contextSamples.at(detectedEndIndex).sourceLine,
            rightEndLine);

        // When both sides span the same number of physical lines, the mapping
        // is one-to-one and no text heuristic is necessary. This also ensures
        // every JSCPD-detected physical line is shown on the right.
        if (rightSpanCount == detectedCount) {
            for (int offset = 1; offset < detectedCount - 1; ++offset) {
                mappedRightByContextLine.insert(
                    contextSamples.at(detectedStartIndex + offset).sourceLine,
                    rightStartLine + offset);
            }
            return mappedRightByContextLine;
        }

        // Otherwise there are extra physical lines on the cmp side. Match only
        // the DETECTED context samples in order and allow unrelated cmp lines
        // between them. Never walk outside contextStartLine..contextEndLine.
        int cursor = rightStartLine + 1;
        const int finalInteriorLine = rightEndLine - 1;

        for (int i = detectedStartIndex + 1;
             i < detectedEndIndex; ++i) {
            int foundLine = 0;
            for (int line = cursor; line <= finalInteriorLine; ++line) {
                if (sampleMatchesRight(contextSamples.at(i), line)) {
                    foundLine = line;
                    break;
                }
            }

            if (foundLine > 0) {
                mappedRightByContextLine.insert(
                    contextSamples.at(i).sourceLine,
                    foundLine);
                cursor = foundLine + 1;
            }
        }

        return mappedRightByContextLine;
    };

    static const QRegularExpression displayHunkPrefixPattern(
        R"(^(hunk_\d+)_(additions|deletions)(?:\.[^/\\]+)?$)",
        QRegularExpression::CaseInsensitiveOption
        );

    for (int i = 0; i < contextDups.size(); ++i)
    {
        const QJsonValue v = contextDups.at(i);
        if (!v.isObject()) {
            continue;
        }

        const QJsonObject dup = v.toObject();
        const int linesTotal = dup.value("lines").toInt();

        const QJsonObject firstObj = dup.value("firstFile").toObject();
        const QJsonObject secondObj = dup.value("secondFile").toObject();

        const QString firstName =
            normalizePath(firstObj.value("name").toString());
        const QString secondName =
            normalizePath(secondObj.value("name").toString());

        if (!passesHunkFilter(firstName, secondName,
                              contextUsingRepoReport, hunkName)) {
            continue;
        }

        QString firstAddKey;
        QString firstDelKey;
        QString secondAddKey;
        QString secondDelKey;

        const bool firstIsContext =
            contextHunkDisplayKeys(firstName, firstAddKey, firstDelKey);
        const bool secondIsContext =
            contextHunkDisplayKeys(secondName, secondAddKey, secondDelKey);

        // A usable context match is exactly one dedicated hunk_N_context file
        // paired with the divergent cmp source.
        if (firstIsContext == secondIsContext) {
            continue;
        }

        QJsonObject contextObj;
        QJsonObject cmpObj;
        QString contextReportedName;
        QString additionsKey;
        QString deletionsKey;

        if (firstIsContext && reportedPathMatchesRight(secondName, keys)) {
            contextObj = firstObj;
            cmpObj = secondObj;
            contextReportedName = firstName;
            additionsKey = firstAddKey;
            deletionsKey = firstDelKey;
        } else if (secondIsContext && reportedPathMatchesRight(firstName, keys)) {
            contextObj = secondObj;
            cmpObj = firstObj;
            contextReportedName = secondName;
            additionsKey = secondAddKey;
            deletionsKey = secondDelKey;
        } else {
            continue;
        }

        const bool hasDeletionHunk =
            leftHunkIndexByFileName.contains(deletionsKey);
        const bool hasAdditionHunk =
            leftHunkIndexByFileName.contains(additionsKey);

        if (!hasDeletionHunk && !hasAdditionHunk) {
            continue;
        }

        // Extract one hunk identity. The context signal belongs to this hunk,
        // not to either projection independently.
        QString displayKeyForPrefix =
            hasAdditionHunk ? additionsKey : deletionsKey;
        const QRegularExpressionMatch prefixMatch =
            displayHunkPrefixPattern.match(displayKeyForPrefix);
        if (!prefixMatch.hasMatch()) {
            continue;
        }
        const QString contextHunkPrefix =
            prefixMatch.captured(1).toCaseFolded();

        // Context is a strict fallback. Prefer the saved per-threshold audit
        // because it records normal matches found before other hunks continued
        // to lower thresholds. Fall back to the current report only for legacy
        // results that do not have directChangeClone metadata.
        bool hunkHasNormalDirect = false;
        if (hasDirectCloneAuditMetadata) {
            hunkHasNormalDirect =
                normalDirectCloneHunks.contains(contextHunkPrefix);
        } else {
            const bool deletionHasDirect = hasDeletionHunk &&
                classificationClonePresenceByHunk.value(deletionsKey, false);
            const bool additionHasDirect = hasAdditionHunk &&
                classificationClonePresenceByHunk.value(additionsKey, false);
            hunkHasNormalDirect = deletionHasDirect || additionHasDirect;
        }

        // A direct normal match still wins classification, but it should not
        // hide useful shared context from the human reviewer.  In that case
        // the dedicated context match is visualization-only and receives a
        // lighter evidence flag.  It never feeds back into MO/ED decisions.
        const int contextEvidenceFlag = hunkHasNormalDirect
            ? SupportingContextEvidence
            : ContextEvidence;

        int contextStartLine = 0;
        int contextStartCol = 0;
        int contextEndLine = 0;
        int contextEndCol = 0;

        int rightStartLine = 0;
        int rightStartCol = 0;
        int rightEndLine = 0;
        int rightEndCol = 0;

        computeRangeFromDup(contextObj, linesTotal,
                            contextStartLine, contextStartCol,
                            contextEndLine, contextEndCol, 0);
        computeRangeFromDup(cmpObj, linesTotal,
                            rightStartLine, rightStartCol,
                            rightEndLine, rightEndCol, 0);

        // The right-side source may contain the hunk's changed lines between
        // two context runs.  Count those lines from both displayed projections
        // so the mapper can search far enough past the JSCPD seed span without
        // scanning the entire source file.
        const std::function<int(const QString &)> countChangedLinesForDisplayKey =
            [this](const QString &displayKey) -> int {
            if (!leftHunkIndexByFileName.contains(displayKey)) {
                return 0;
            }

            const int idx = leftHunkIndexByFileName.value(displayKey);
            if (idx < 0 || idx >= leftHunkRanges.size()) {
                return 0;
            }

            const bool additionsSide = displayKey.contains("_additions");
            const bool deletionsSide = displayKey.contains("_deletions");
            if (!additionsSide && !deletionsSide) {
                return 0;
            }

            QFile displayedFile(leftHunkRanges.at(idx).filePath);
            if (!displayedFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                return 0;
            }

            int changedLines = 0;
            QTextStream in(&displayedFile);
            while (!in.atEnd()) {
                const QString line = in.readLine();
                if (additionsSide &&
                    line.startsWith('+') && !line.startsWith("+++")) {
                    ++changedLines;
                } else if (deletionsSide &&
                           line.startsWith('-') && !line.startsWith("---")) {
                    ++changedLines;
                }
            }
            return changedLines;
        };

        int contextGapBudget = 0;
        if (hasAdditionHunk) {
            contextGapBudget += countChangedLinesForDisplayKey(additionsKey);
        }
        if (hasDeletionHunk) {
            contextGapBudget += countChangedLinesForDisplayKey(deletionsKey);
        }

        const QMap<int, int> rightLinesByContextLine =
            mapContextRangeToRight(
                contextReportedName,
                repoBaseAbs,
                contextStartLine,
                contextEndLine,
                rightStartLine,
                rightEndLine,
                contextGapBudget);

        for (QMap<int, int>::const_iterator it = rightLinesByContextLine.constBegin();
             it != rightLinesByContextLine.constEnd(); ++it) {
            addEvidence(rightEvidenceByLine, it.value(), contextEvidenceFlag);
            if (firstRightDupLine == 0) {
                firstRightDupLine = it.value();
            }
        }

        // The context is shared. If both additions and deletions are displayed,
        // map the same context source lines onto both instead of arbitrarily
        // choosing the deletion projection.
        QStringList targetDisplayKeys;
        if (hasAdditionHunk) {
            targetDisplayKeys.append(additionsKey);
        }
        if (hasDeletionHunk) {
            targetDisplayKeys.append(deletionsKey);
        }

        if (!leftHasText) {
            continue;
        }

        for (const QString &targetDisplayKey : targetDisplayKeys) {
            if (rightLinesByContextLine.isEmpty()) {
                continue;
            }

            // Highlight only the context lines that the dedicated JSCPD
            // context report actually detected. Mapping on the cmp side may
            // skip unrelated physical lines, but it must never expand the
            // evidence range on the left.
            const QMap<int, int> leftLinesByContextLine =
                mapContextRangeToCombinedLeft(
                    targetDisplayKey,
                    contextStartLine,
                    contextEndLine);

            for (QMap<int, int>::const_iterator it = leftLinesByContextLine.constBegin();
                 it != leftLinesByContextLine.constEnd(); ++it) {
                const int contextLine = it.key();
                const int mappedRightLine =
                    rightLinesByContextLine.value(contextLine, 0);

                // A displayed context line is a real visual pair: only paint it
                // when we also know where it lands on cmp.  This guarantees
                // that every blue context line can participate in click/jump
                // navigation instead of creating a highlighted dead end.
                if (!rightHasText || mappedRightLine <= 0) {
                    continue;
                }

                const int combinedLine = it.value();
                addEvidence(leftEvidenceByLine,
                            combinedLine,
                            contextEvidenceFlag);

                const QTextBlock leftBlock =
                    leftEdit->document()->findBlockByNumber(combinedLine - 1);
                const QTextBlock rightBlock =
                    rightEdit->document()->findBlockByNumber(mappedRightLine - 1);
                if (!leftBlock.isValid() || !rightBlock.isValid()) {
                    continue;
                }

                DuplicateRegion reg;
                reg.leftStartPos = leftBlock.position();
                // DuplicateRegion uses an exclusive end position.  Include the
                // paragraph separator so clicking at the visible end of a
                // highlighted line still triggers navigation, while the next
                // block's first character remains outside this region.
                reg.leftEndPos =
                    leftBlock.position() + qMax(1, leftBlock.length());
                reg.rightStartPos = rightBlock.position();
                reg.rightEndPos =
                    rightBlock.position() + qMax(1, rightBlock.length());
                duplicateRegions.push_back(reg);
            }
        }
    }

    const QColor additionColor(180, 255, 180);
    const QColor deletionColor(255, 180, 180);
    const QColor contextColor(180, 220, 255);
    const QColor supportingContextColor(220, 238, 255);
    const QColor overlapColor(255, 190, 220);

    const std::function<void(CodeDocument *, const QHash<int, int> &)> applyLineEvidence =
        [this, &additionColor, &deletionColor, &contextColor,
         &supportingContextColor, &overlapColor]
        (CodeDocument *editor, const QHash<int, int> &evidenceByLine) {
        for (QHash<int, int>::const_iterator it = evidenceByLine.constBegin();
             it != evidenceByLine.constEnd(); ++it) {
            const int mask = it.value();
            QColor color = contextColor;

            const bool bothSides =
                (mask & BothEvidence) != 0 ||
                ((mask & AdditionEvidence) != 0 &&
                 (mask & DeletionEvidence) != 0);

            // Direct change evidence wins over context evidence.  Blue is
            // reserved for the dedicated hunk_N_context fallback detector;
            // the normal additions/deletions report can never manufacture it.
            if (bothSides) {
                color = overlapColor;
            } else if ((mask & AdditionEvidence) != 0) {
                color = additionColor;
            } else if ((mask & DeletionEvidence) != 0) {
                color = deletionColor;
            } else if ((mask & ContextEvidence) != 0) {
                color = contextColor;
            } else if ((mask & SupportingContextEvidence) != 0) {
                color = supportingContextColor;
            }

            highlightRange(editor, it.key(), 1, it.key(), 1, color);
        }
    };

    if (leftHasText) {
        applyLineEvidence(leftEdit, leftEvidenceByLine);
    }
    if (rightHasText) {
        applyLineEvidence(rightEdit, rightEvidenceByLine);
    }

    applySelections(leftEdit);
    applySelections(rightEdit);

    if (firstRightDupLine > 0 && rightEdit != NULL)
        jumpToLine(rightEdit, firstRightDupLine);

}

void ResultsViewerController::onCursorPositionChanged(){
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
            break;
        }
    }
}

bool ResultsViewerController::matchesFilePair(const QString &firstName, const QString &secondName, const JscpdMatchKeys &keys){
    const QString firstHunkKey = displayedHunkKey(firstName);
    const QString secondHunkKey = displayedHunkKey(secondName);

    const bool firstIsLeft = !firstHunkKey.isEmpty() &&
                             leftHunkIndexByFileName.contains(firstHunkKey);
    const bool secondIsLeft = !secondHunkKey.isEmpty() &&
                              leftHunkIndexByFileName.contains(secondHunkKey);

    const bool normalOrder = firstIsLeft &&
                             reportedPathMatchesRight(secondName, keys);
    const bool reversedOrder = reportedPathMatchesRight(firstName, keys) &&
                               secondIsLeft;

    return normalOrder || reversedOrder;
}

bool ResultsViewerController::passesHunkFilter(const QString &firstName, const QString &secondName, bool usingRepoReport, const QString &hunkName){
    if (usingRepoReport)
    {
        return true;
    }

    // v5 may emit only basenames even when the report was opened from the PR
    // fallback location. In that case the later exact file-pair check is the
    // only reliable discriminator.
    if (!firstName.contains('/') && !secondName.contains('/')) {
        return true;
    }

    bool inHunkFirst = false;
    bool inHunkSecond = false;

    if (firstName.contains("/" + hunkName + "/") || firstName.startsWith(hunkName + "/"))
    {
        inHunkFirst = true;
    }

    if (secondName.contains("/" + hunkName + "/") || secondName.startsWith(hunkName + "/"))
    {
        inHunkSecond = true;
    }

    if (!inHunkFirst || !inHunkSecond)
    {
        return false;
    }

    return true;
}

JscpdMatchKeys ResultsViewerController::buildMatchKeys(const QString &repoBaseAbs, const QString &hunkName, const QString &githubPathName){
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

bool ResultsViewerController::parseDuplicates(QFile &rf, QJsonArray &duplicatesOut){
    QByteArray bytes = rf.readAll();

    QJsonParseError perr;
    QJsonDocument doc = QJsonDocument::fromJson(bytes, &perr);

    if (perr.error != QJsonParseError::NoError)
    {
        return false;
    }

    if (!doc.isObject())
    {
        return false;
    }

    QJsonObject root = doc.object();
    duplicatesOut = root.value("duplicates").toArray();
    return true;
}

// Opens the first report under root that exists, trying each location in order.
static bool openFirstReport(const QString &root, const QStringList &reportLocations, QFile &rf){
    for (const QString &relativePath : reportLocations) {
        rf.close();
        rf.setFileName(QDir(root).filePath(relativePath));

        if (rf.open(QIODevice::ReadOnly)) {
            return true;
        }
    }

    return false;
}

bool ResultsViewerController::openJscpdReport(const QString &repoBaseAbs, const QString &prRootAbs, QFile &rf, bool &usingRepoReport){
    const QStringList reportLocations = {
        // New hunk-level classifier preserves the exact normal JSCPD records
        // from the threshold at which each individual hunk resolved. Prefer
        // that merged report so visualization matches classification.
        "reports/jscpd-classification-report.json",
        "reports/jscpd-report.json",
        // Compatibility with reports produced before the jscpd update.
        "reports/html/jscpd-report.json"
    };

    usingRepoReport = false;

    if (openFirstReport(repoBaseAbs, reportLocations, rf)) {
        usingRepoReport = true;
        return true;
    }

    return openFirstReport(prRootAbs, reportLocations, rf);
}

bool ResultsViewerController::openJscpdContextReport(const QString &repoBaseAbs,
                                                 const QString &prRootAbs,
                                                 QFile &rf,
                                                 bool &usingRepoReport){
    const QStringList reportLocations = {
        "reports/jscpd-context-report.json"
    };

    usingRepoReport = false;

    if (openFirstReport(repoBaseAbs, reportLocations, rf)) {
        usingRepoReport = true;
        return true;
    }

    return openFirstReport(prRootAbs, reportLocations, rf);
}
