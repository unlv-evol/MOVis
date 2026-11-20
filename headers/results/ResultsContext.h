#ifndef RESULTS_CONTEXT_H
#define RESULTS_CONTEXT_H

#include <QString>

// The header strip shown above the two code panes in the results viewer.
//
// ResultsViewer.qml reads every field through the ResultsViewerController property
// of the same name, so renaming a field here means renaming it there too.
struct ResultsContext {
    QString prTitle = QStringLiteral("-");
    QString prUrl = QStringLiteral("-");

    // Absolute paths behind the "PR Results" and "File Results" links. Empty
    // whenever there is no file to open, which is what QML checks before it
    // enables the link.
    QString prResultsPath;
    QString fileResultsPath;

    // Classification of the file currently shown on the right.
    QString fileClassification = QStringLiteral("-");

    // Per-hunk detail, only shown once a single hunk has been selected.
    QString hunkSideClassifications =
        QStringLiteral("Hunk_Add_Class: - | Hunk_Del_Class: -");
    bool hunkSideClassificationsVisible = false;

    // Populated for files that failed to classify.
    QString errorDetails = QStringLiteral("-");
    bool errorDetailsVisible = false;

    // The right pane holds comparison files for most classifications, but
    // whole result records for the terminal ones.
    QString rightSelectorLabel = QStringLiteral("File");
};

#endif // RESULTS_CONTEXT_H
