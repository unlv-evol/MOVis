#ifndef HUNKRANGE_H
#define HUNKRANGE_H

#include <QString>

struct HunkRange {
    int startBlock;
    int endBlock;
    QString hunkName;
    QString filePath;
};

#endif // HUNKRANGE_H
