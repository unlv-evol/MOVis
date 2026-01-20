#ifndef JSCPDMATCHKEYS_H
#define JSCPDMATCHKEYS_H
#include <QString>

struct JscpdMatchKeys
{
    QString leftRelRepo;
    QString rightRelRepo;

    QString leftRelPR;
    QString rightRelPR;

    QString leftFileName;
    QString rightFileName;
};

#endif // JSCPDMATCHKEYS_H
