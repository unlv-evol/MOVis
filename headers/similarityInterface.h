#ifndef SIMILARITYINTERFACE_H
#define SIMILARITYINTERFACE_H

#include <QWidget>
#include <QPlainTextEdit>
#include <QGroupBox>
#include <QComboBox>
#include <QSplitter>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include "prSimilarityParser.h"
#include "LineNumberHelpers.h"
#include "structures/jscpdMatchKeys.h"
#include "structures/HighlightSpec.h"
#include "structures/HunkRange.h"
#include "structures/InformedLines.h"

struct DuplicateRegion {
    int leftStartPos;
    int leftEndPos;
    int rightStartPos;
    int rightEndPos;
};

class similarityInterface: public QWidget{
    Q_OBJECT
    public:
        QString currentPath;

        similarityInterface(QWidget* parent = nullptr, QString rootFolder = ""); // Done
        void jumpToLine(QPlainTextEdit *editor, int lineNumber); // Done

        bool setSimilarityResultsFile(const QString &resultsPath, QString *errorOut = nullptr);

        // Convenience wrapper for querying similarity given a file + hunk name.
        // Returns false if no match is found or if no results file has been loaded.
        bool querySimilarity(const QString &fileName,
                             const QString &hunkName,
                             double *similarityOut = nullptr,
                             QString *classificationOut = nullptr);

        void setPRHyperlink(QGroupBox *group, const QString &title, const QString &locationUrl); // Done

    private slots:
        void highlightSimilarLines(); // Done

        void onClassificationChanged(int index); // Classification: ED/MO/NA/Etc.. Done
        void onPRChanged(int index);             // PR specific to the classification Done

        void switchFilesOnLeft(int index);      // Done
        void switchFilesOnRight(int index);     // Done

        void onCursorPositionChanged();  // Done

    private:
        QString rootDirectory;
        QString currentFile;
        QMap<QString, QSet<QString>> classificationToPRs;
        QHash<QString, QString> srcFileToHunk;
        QVector<DuplicateRegion> duplicateRegions;

        void initializeHighlights(QVector<HighlightSpec> &specs); // Done

        prSimilarityParser similarityParser;
        QString similarityResultsPath; // Path to the "pr_results.txt"
        bool similarityLoaded = false;

        void updateSimilarityOnPair();

        void loadIntoLeftOrRight(QPlainTextEdit *targetEditor); // Done
        bool loadPathIntoLeftOrRight(const QString &path, QPlainTextEdit *targetEditor); // Done

        void highlightRange(QPlainTextEdit *editor, int startLine, int startColumn,
                            int endLine, int endColumn, const QColor &color); // Done
        void applySelections(QPlainTextEdit *editor); // Done
        int toPos(QPlainTextEdit *editor, int lineOneBased, int colOneBased); // Done

        QWidget* root;               // container with VBox (GroupBox on top + splitter below)
        QSplitter* splitter;

        QHash<int, QColor> rightLineCheck;

        // Top controls (single set)
        QGroupBox* topGroup;
        QLabel* classificationLabel;
        QComboBox* classificationCombo;
        QLabel* prLabel;
        QComboBox* prCombo;

        // LEFT panel (selector + editor)
        QWidget* leftPanel;
        QComboBox* leftFileSelector;
        LineNumberHelpers* leftEdit;
        QVector<HunkRange> leftHunkRanges;
        QHash<QString, int> leftHunkIndexByFileName;
        QVector<InformedLines> leftNums;
        int totalFiles;

        // RIGHT panel (selector + editor)
        QWidget* rightPanel;
        QComboBox* rightFileSelector;
        LineNumberHelpers* rightEdit;

        // ---------- State ----------
        QHash<int, bool> rightFileCheck;
        QString leftPath, rightPath;
        QString leftOpenPath;

        QVector<QTextEdit::ExtraSelection> leftSelections;
        QVector<QTextEdit::ExtraSelection> rightSelections;

        QVector<HighlightSpec> startupSpecs;
        bool startupHighlightsRan = false;

        // Files bucketed by Classification -> PR -> [paths]
        QMap<QString, QMap<QString, QStringList>> fileBuckets;

        // Fixed list of classification options
        QStringList classificationOptions() const { return {"MO"}; }

        // Example PRs for each classification (seeded at startup; you can replace later)
        QMap<QString, QStringList> classificationToDefaultPRs;

        // Internal Helpers
        void resetComparisonState();
        void clearLeftPane(bool clearSelector);
        void setHunkLabel(const QString &text);
        void setHunkLabelFromRepoBase(QString repoBaseAbs);
        bool openLeftFromCurrentSelection();
        QString extractCurrentFileFromCmpPath(const QString &cmpPath);

        void clearStartupHighlightState();
        bool parseDuplicates(QFile &rf, QJsonArray &duplicatesOut);
        bool getRepoContextFromCmp(QString &repoBaseAbs, QString &hunkName, QString &githubPathName);
        bool openJscpdReport(const QString &repoBaseAbs, const QString &prRootAbs, QFile &rf,
                             bool &usingRepoReport);
        bool matchesFilePair(const QString &firstName, const QString &secondName, const JscpdMatchKeys &keys);
        JscpdMatchKeys buildMatchKeys(const QString &repoBaseAbs, const QString &hunkName,
                                      const QString &githubPathName);
        bool passesHunkFilter(const QString &firstName, const QString &secondName,
                              bool usingRepoReport, const QString &hunkName);

        void loadAllLeftHunks(const QStringList& srcFiles);
        int mapHunkLineToCombinedLeft(const QString& hunkFileName, int jsLine) const;
};

#endif // SIMILARITYINTERFACE_H
