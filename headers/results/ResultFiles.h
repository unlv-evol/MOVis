#ifndef RESULT_FILES_H
#define RESULT_FILES_H

#include <QSet>
#include <QString>

// Reading and locating the JSON files a classification run leaves behind.
//
// A result folder is laid out as:
//
//     <root>/<pr>_<classification>/pr_results.json
//     <root>/<pr>_<classification>/<file classification>/<file>/results.json
//
// so the viewer has to go looking for both the per-PR record and the per-file
// one for whatever the user currently has selected.

// One top-level string field out of an individual results.json.
QString individualResultField(const QString &resultsPath,
                              const QString &fieldName);

// Human-readable explanation of why a file could not be classified,
// derived from the error fields the run recorded.
QString individualResultErrorSummary(const QString &resultsPath);

// Hunk ids that the run marked as direct clones. `hasDirectCloneMetadata`
// comes back false for older results that never recorded the flag, which
// callers treat as "unknown" rather than "none".
QSet<QString> directCloneHunksFromIndividualResult(
    const QString &resultsPath,
    bool &hasDirectCloneMetadata);

// Absolute path if the file is really there, empty otherwise. The viewer
// only offers links to files it can actually open.
QString existingResultsFilePath(const QString &filePath);

// pr_results.json for a selection, falling back to the .txt name older
// runs used. Empty when neither exists.
QString prResultsFileForSelection(const QString &rootDirectory,
                                  const QString &pr,
                                  const QString &classification);

// results.json belonging to whichever file is selected in the right pane.
QString individualResultsForSelectedPath(const QString &rootDirectory,
                                         const QString &pr,
                                         const QString &prClassification,
                                         const QString &selectedPath);

// CC, NE and ERROR files were never compared against anything, so the
// viewer lists their result records instead of comparison files.
bool isTerminalVisualizerClassification(const QString &classification);

#endif // RESULT_FILES_H
