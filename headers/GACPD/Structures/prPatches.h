#ifndef PRPATCHES_H
#define PRPATCHES_H

#include <QStringList>
#include <QSet>

struct prPatches{
    QStringList prs;
    QStringList title;
    double seconds;
};

struct windowSliceResult{
    prPatches patches;
    int issueCount = 0;
};

#endif // PRPATCHES_H
