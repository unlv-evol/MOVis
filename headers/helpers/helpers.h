#ifndef HELPERS_H
#define HELPERS_H

#include <QStringList>
#include <QColor>
#include <QSet>
#include <QJsonObject>
#include <QRegularExpression>

QStringList sortedSet(const QSet<QString>& set);
QColor colorSelection(int i);
void computeRangeFromDup(const QJsonObject &fileObj, int linesTotal,
                         int &startLine, int &startCol, int &endLine, int &endCol, bool isLeft);
int suffixRank(const QString& suffix);
bool compareHunkPaths(const QString& leftPath,
                             const QString& rightPath,
                             const QRegularExpression& regex);
void sortHunkFiles(QStringList& paths);
#endif // HELPERS_H
