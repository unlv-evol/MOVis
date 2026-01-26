#ifndef PRFILEREPORT_H
#define PRFILEREPORT_H

#include <QString>
#include <QHash>

// Struct created for more information storage in the future
struct HunkInfo {
    double similarityPercent = -1.0; // -1 means not found
};

struct FileReport {
    QString analyzedPath;            // full path from "Similarity analysis for:"
    QString overallClassification;   // e.g., MO / ED / etc.
    QHash<QString, HunkInfo> hunks;  // key: hunk file name (e.g., src/hunk_1_additions.scala)
};

#endif // PRFILEREPORT_H
