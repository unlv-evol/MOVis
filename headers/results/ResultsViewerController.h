#ifndef RESULTS_VIEWER_CONTROLLER_H
#define RESULTS_VIEWER_CONTROLLER_H

#include <QColor>
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QMap>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QUrl>
#include <QVector>

#include "headers/prSimilarityParser.h"
#include "headers/results/CodeDocument.h"
#include "headers/results/ResultsContext.h"
#include "headers/results/SelectionModel.h"
#include "headers/structures/HunkRange.h"
#include "headers/structures/InformedLines.h"
#include "headers/structures/jscpdMatchKeys.h"

// One highlighted line in the left pane and the line it duplicates on the
// right, in character positions. Clicking inside the left range scrolls the
// right pane to its counterpart.
struct DuplicateRegion {
    int leftStartPos = 0;
    int leftEndPos = 0;
    int rightStartPos = 0;
    int rightEndPos = 0;
};

// Backing object for ResultsViewer.qml: one window onto one result folder.
//
// The viewer has four jobs, and the implementation is split across one file
// per job so a change lands in an obvious place:
//
//   Selection     ResultsViewerController_Selection.cpp
//                 classification -> pull request -> file, and PR search
//   Panes         ResultsViewerController_Panes.cpp
//                 loading files into the two code panes, hunks, text search
//   Highlighting  ResultsViewerController_Highlighting.cpp
//                 reading the jscpd report, tinting lines, jumping between panes
//   Context       ResultsViewerController_Context.cpp
//                 the header strip above the panes
//
// The declarations below are grouped in the same order.
class ResultsViewerController : public QObject
{
    Q_OBJECT

    // ---- Selection -----------------------------------------------------------
    Q_PROPERTY(QString rootDirectory READ rootDirectoryPath WRITE loadRootDirectory
               NOTIFY stateChanged)
    Q_PROPERTY(QStringList classifications READ classifications NOTIFY stateChanged)
    Q_PROPERTY(int classificationIndex READ classificationIndex
               WRITE selectClassification NOTIFY stateChanged)
    Q_PROPERTY(QStringList pullRequests READ pullRequests NOTIFY stateChanged)
    Q_PROPERTY(QStringList allPullRequests READ availablePullRequests NOTIFY stateChanged)
    Q_PROPERTY(int pullRequestIndex READ pullRequestIndex
               WRITE selectPullRequest NOTIFY stateChanged)
    Q_PROPERTY(QStringList hunkChoices READ hunkChoices NOTIFY stateChanged)
    Q_PROPERTY(int hunkIndex READ hunkIndex WRITE selectHunk NOTIFY stateChanged)
    Q_PROPERTY(bool hunkChoicesEnabled READ hunkChoicesEnabled NOTIFY stateChanged)
    Q_PROPERTY(QStringList rightFileChoices READ rightFileChoices NOTIFY stateChanged)
    Q_PROPERTY(int rightFileIndex READ rightFileIndex
               WRITE selectRightFile NOTIFY stateChanged)

    // ---- Panes ---------------------------------------------------------------
    Q_PROPERTY(QString leftDocument READ leftDocument NOTIFY stateChanged)
    Q_PROPERTY(QString rightDocument READ rightDocument NOTIFY stateChanged)
    Q_PROPERTY(QString leftLineNumbers READ leftLineNumbers NOTIFY stateChanged)
    Q_PROPERTY(QString rightLineNumbers READ rightLineNumbers NOTIFY stateChanged)
    Q_PROPERTY(QVariantList leftLineBands READ leftLineBands NOTIFY stateChanged)
    Q_PROPERTY(QVariantList rightLineBands READ rightLineBands NOTIFY stateChanged)

    // ---- Context strip -------------------------------------------------------
    Q_PROPERTY(QString prTitle READ prTitle NOTIFY stateChanged)
    Q_PROPERTY(QString prUrl READ prUrl NOTIFY stateChanged)
    Q_PROPERTY(QString prResultsPath READ prResultsPath NOTIFY stateChanged)
    Q_PROPERTY(QString fileResultsPath READ fileResultsPath NOTIFY stateChanged)
    Q_PROPERTY(QString fileClassification READ fileClassification NOTIFY stateChanged)
    Q_PROPERTY(QString hunkSideClassifications READ hunkSideClassifications
               NOTIFY stateChanged)
    Q_PROPERTY(bool hunkSideClassificationsVisible
               READ hunkSideClassificationsVisible NOTIFY stateChanged)
    Q_PROPERTY(QString errorDetails READ errorDetails NOTIFY stateChanged)
    Q_PROPERTY(bool errorDetailsVisible READ errorDetailsVisible NOTIFY stateChanged)
    Q_PROPERTY(QString rightSelectorLabel READ rightSelectorLabel NOTIFY stateChanged)

public:
    explicit ResultsViewerController(QObject *parent = nullptr,
                                     QString rootFolder = {});

    // ---- Selection -----------------------------------------------------------
    QString rootDirectoryPath() const { return rootDirectory; }
    QStringList classifications() const { return classificationCombo->labels(); }
    int classificationIndex() const { return classificationCombo->currentIndex(); }
    QStringList pullRequests() const { return prCombo->labels(); }
    QStringList availablePullRequests() const { return allPRNumbers(); }
    int pullRequestIndex() const { return prCombo->currentIndex(); }
    QStringList hunkChoices() const { return leftFileSelector->labels(); }
    int hunkIndex() const { return leftFileSelector->currentIndex(); }
    bool hunkChoicesEnabled() const { return leftFileSelector->isEnabled(); }
    QStringList rightFileChoices() const { return rightFileSelector->labels(); }
    int rightFileIndex() const { return rightFileSelector->currentIndex(); }

    Q_INVOKABLE void loadRootDirectory(const QString &path);
    Q_INVOKABLE void selectClassification(int index);
    Q_INVOKABLE void selectPullRequest(int index);
    Q_INVOKABLE void selectHunk(int index);
    Q_INVOKABLE void selectRightFile(int index);
    Q_INVOKABLE void searchPullRequest(const QString &text);
    Q_INVOKABLE void clearPullRequestSearch();

    // ---- Panes ---------------------------------------------------------------
    QString leftDocument() const { return leftEdit->toHtml(); }
    QString rightDocument() const { return rightEdit->toHtml(); }
    QString leftLineNumbers() const { return leftEdit->lineNumbersText(); }
    QString rightLineNumbers() const { return rightEdit->lineNumbersText(); }
    QVariantList leftLineBands() const { return leftEdit->lineBands(); }
    QVariantList rightLineBands() const { return rightEdit->lineBands(); }

    Q_INVOKABLE void findLeftTextInRight(const QString &selectedText,
                                         const QString &searchText);
    Q_INVOKABLE void findRightTextInLeft(const QString &selectedText,
                                         const QString &searchText);
    Q_INVOKABLE void leftCursorMoved(int position);
    Q_INVOKABLE QUrl localFileUrl(const QString &path) const;

    // ---- Context strip -------------------------------------------------------
    QString prTitle() const { return context.prTitle; }
    QString prUrl() const { return context.prUrl; }
    QString prResultsPath() const { return context.prResultsPath; }
    QString fileResultsPath() const { return context.fileResultsPath; }
    QString fileClassification() const { return context.fileClassification; }
    QString hunkSideClassifications() const { return context.hunkSideClassifications; }
    bool hunkSideClassificationsVisible() const { return context.hunkSideClassificationsVisible; }
    QString errorDetails() const { return context.errorDetails; }
    bool errorDetailsVisible() const { return context.errorDetailsVisible; }
    QString rightSelectorLabel() const { return context.rightSelectorLabel; }

    bool setSimilarityResultsFile(const QString &resultsPath,
                                  QString *errorOut = nullptr);
    bool querySimilarity(const QString &fileName,
                         const QString &hunkName,
                         double *similarityOut = nullptr,
                         QString *classificationOut = nullptr);

signals:
    void stateChanged();
    void messageRequested(const QString &title, const QString &message);

    // Asks the QML pane to select a range and scroll it into view.
    void leftNavigationRequested(int selectionStart, int selectionEnd);
    void rightNavigationRequested(int selectionStart, int selectionEnd);

    // A different file or hunk is now showing, so both panes should go back to
    // their first line rather than keeping the previous scroll position.
    void panesReset();

    void searchFieldsCleared();

private slots:
    // ---- Selection -----------------------------------------------------------
    void onClassificationChanged(int index);
    void onPRChanged(int index);

    // ---- Panes ---------------------------------------------------------------
    void onHunkViewChanged(int index);
    void switchFilesOnRight(int index);

    // ---- Highlighting --------------------------------------------------------
    void highlightSimilarLines();
    void onCursorPositionChanged();

private:
    // ---- Selection -----------------------------------------------------------
    void createStateObjects();
    void connectStateObjects();

    QStringList allPRNumbers() const;
    QString classificationForPR(const QString &pr) const;
    QString normalizePRSearchInput(const QString &text) const;
    bool selectPRByNumber(const QString &pr);

    // The classifications a run can produce, in the order the selector lists
    // them. Only the ones present in the loaded folder are offered.
    QStringList classificationOptions() const
    {
        return {"MO", "ED", "NA", "CC", "NE", "ERROR"};
    }

    QString rootDirectory;
    QMap<QString, QSet<QString>> classificationToPRs;

    SelectionModel *classificationCombo = nullptr;
    SelectionModel *prCombo = nullptr;
    SelectionModel *leftFileSelector = nullptr;
    SelectionModel *rightFileSelector = nullptr;

    // Last pull request the user searched for, in its resolved form.
    QString prSearchText;

    // ---- Panes ---------------------------------------------------------------
    // The work behind onHunkViewChanged() and switchFilesOnRight(). They are
    // separate so those slots can always reset the panes afterwards, whichever
    // way the work bailed out.
    void applyHunkSelection(int index);
    void applyRightFileSelection(int index);

    bool loadPathIntoLeftOrRight(const QString &path, CodeDocument *targetEditor);
    void loadAllLeftHunks(const QStringList &sourceFiles);
    void populateHunkViewSelector(const QStringList &sourceFiles);
    void clearLeftPane(bool clearSelector);
    void clearSearchFields();

    // Where the selected comparison file sits inside the result folder, which
    // is what tells the viewer which hunks and reports belong to it.
    bool getRepoContextFromCmp(QString &repoBaseAbs,
                               QString &hunkName,
                               QString &githubPathName);

    int toPos(CodeDocument *editor, int lineOneBased, int columnOneBased);
    void jumpToLine(CodeDocument *editor, int lineNumber);

    QString normalizedSearchText(const QString &selectedText,
                                 const QString &typedText) const;
    bool findTextInEditor(CodeDocument *editor, const QString &text);

    CodeDocument *leftEdit = nullptr;
    CodeDocument *rightEdit = nullptr;
    QString leftPath;
    QString rightPath;

    // The left pane is every hunk file concatenated, so it needs a map back to
    // the individual hunks and their original line numbers.
    QStringList currentLeftHunkFiles;
    QVector<HunkRange> leftHunkRanges;
    QHash<QString, int> leftHunkIndexByFileName;
    QVector<InformedLines> leftNums;
    int selectedHunkNumber = 0;

    // ---- Highlighting --------------------------------------------------------
    void highlightRange(CodeDocument *editor,
                        int startLine,
                        int startColumn,
                        int endLine,
                        int endColumn,
                        const QColor &color);
    void applySelections(CodeDocument *editor);
    void resetComparisonState();

    // Opening and filtering the jscpd report behind the current selection.
    bool openJscpdReport(const QString &repoBaseAbs,
                         const QString &prRootAbs,
                         QFile &file,
                         bool &usingRepoReport);
    bool openJscpdContextReport(const QString &repoBaseAbs,
                                const QString &prRootAbs,
                                QFile &file,
                                bool &usingRepoReport);
    bool parseDuplicates(QFile &file, QJsonArray &duplicatesOut);
    JscpdMatchKeys buildMatchKeys(const QString &repoBaseAbs,
                                  const QString &hunkName,
                                  const QString &githubPathName);
    bool matchesFilePair(const QString &firstName,
                         const QString &secondName,
                         const JscpdMatchKeys &keys);
    bool passesHunkFilter(const QString &firstName,
                          const QString &secondName,
                          bool usingRepoReport,
                          const QString &hunkName);

    QVector<TextExtraSelection> leftSelections;
    QVector<TextExtraSelection> rightSelections;
    QVector<DuplicateRegion> duplicateRegions;

    // ---- Context strip -------------------------------------------------------
    void setPRHyperlink(const QString &title, const QString &locationUrl);
    void setPRResultsLink(const QString &filePath);
    void setFileResultsLink(const QString &filePath);
    void setRightSelectorLabel(const QString &text);
    void setHunkLabel(const QString &text);
    void setHunkLabelFromRepoBase(QString repoBaseAbs);
    void setErrorDetails(const QString &text);
    void updateSelectedHunkClassificationDisplay();
    void updateSimilarityOnPair();

    ResultsContext context;
    prSimilarityParser similarityParser;
    QString similarityResultsPath;
    bool similarityLoaded = false;
};

#endif // RESULTS_VIEWER_CONTROLLER_H
