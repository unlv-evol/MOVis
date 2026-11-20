#ifndef REPODATEINFO_H
#define REPODATEINFO_H

#include <QString>

struct RepoDateInfo{
    QString mainlineRepo;
    QString variantRepo;
    QString forkDate;
    QString divergenceDate;
    QString cutOffDate;
    int aheadBy;
    int behindBy;
};

#endif // REPODATEINFO_H
