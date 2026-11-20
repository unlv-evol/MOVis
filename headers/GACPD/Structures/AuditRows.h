#ifndef AUDIT_ROWS_H
#define AUDIT_ROWS_H

#include <QString>

// Rows written to the CSV audit files a completed run leaves beside its
// results, one struct per file.

struct PatchRow {
    QString patchNumber;
    QString patchTitle;
};

struct FileClassRow {
    QString mainline;
    QString fork;
    QString prNumber;
    QString filename;
    QString operation;
    QString fileClassification;
    int interesting = 0;
};

struct PatchClassRow {
    QString mainline;
    QString fork;
    QString prNumber;
    QString patchClassification;
    int interesting = 0;
};

#endif // AUDIT_ROWS_H
