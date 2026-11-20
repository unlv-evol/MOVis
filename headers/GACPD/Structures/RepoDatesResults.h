#ifndef REPODATESRESULTS_H
#define REPODATESRESULTS_H

#include <QString>

struct RepoDatesResults {
    QString fullName;
    QString createdAt;
    QString updatedAt;
    QString pushedAt;
    QString defaultBranch;
};

#endif // REPODATESRESULTS_H
