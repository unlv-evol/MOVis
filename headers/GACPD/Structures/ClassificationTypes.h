#ifndef CLASSIFICATION_TYPES_H
#define CLASSIFICATION_TYPES_H

#include <QMap>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QVector>

// Value types shared by the classification run and the helpers it calls.

// The temporary directory laid out for one file's clone detection: the file
// itself, what it is compared against, the patch it came from, and where jscpd
// writes its reports.
struct ClassificationWorkspace {
    QString root;
    QString src;
    QString cmp;
    QString patch;
    QString reports;
};

// What MOVis knows about a file's type. `registered` means jscpd has a format
// for it, which is what decides whether it can be scanned at all.
struct FileExtensionInfo {
    bool isSupported = false;
    QString extension;
    QString language;
    bool registered = false;
};

struct HunkClassificationInfo {
    int hunkNumber = 0;
    QString classification;
    // Per-side classifications are persisted for visualizer transparency.
    // "N/A" means that side does not exist for this hunk; "NA" means the
    // side exists but did not provide classification evidence.
    QString additionsClassification = "N/A";
    QString deletionsClassification = "N/A";
};

// The outcome for one file: its overall classification, the similarity lines
// behind it, and the per-hunk detail.
struct ClassificationResult {
    QString classification;
    QStringList similarityLines;
    QVector<HunkClassificationInfo> hunkClassifications;
};

// Files GitHub reported as added or renamed in a pull request.
struct gitFiles {
    QStringList addedFiles;
    QMap<QString, QString> renamedFiles;
    QVector<QPair<QString, QString>> renamedFileRecords;
};

struct CycleSearchResult {
    bool found = false;
    QStringList cycle;
};

#endif // CLASSIFICATION_TYPES_H
