#ifndef INFORMEDLINES_H
#define INFORMEDLINES_H

#include <QVector>

enum lineTypes {
    CONTEXT,
    DELETED,
    ADDED
};

struct InformedLines{
    QVector<int> leftLineNumbers;
    QVector<lineTypes> leftLineColor;
};

#endif // INFORMEDLINES_H
