#include "headers/helpers/helpers.h"
#include <algorithm>
#include <climits>

QStringList sortedSet(const QSet<QString> &set) {
    QStringList list = set.values();
    list.sort(Qt::CaseInsensitive);
    return list;
}

QColor colorSelection(int i) {
    int hue = (i * 47) % 360;
    QColor c;
    c.setHsl(hue, 160, 140, 150);
    return c;
}

void computeRangeFromDup(const QJsonObject &fileObj, int linesTotal, int &startLine, int &startCol, int &endLine, int &endCol, bool isLeft){
    QJsonObject sLoc = fileObj.value("startLoc").toObject();
    QJsonObject eLoc = fileObj.value("endLoc").toObject();

    int baseLine = sLoc.value("line").toInt(fileObj.value("start").toInt(1));
    int baseCol = sLoc.value("column").toInt(1);

    if (isLeft){
        baseLine = baseLine + 1;
    }

    startLine = baseLine;
    startCol = baseCol;

    endLine = startLine + linesTotal - 1;
    endCol = eLoc.value("column").toInt(startCol);
}


int suffixRank(const QString& suffix){
    if (suffix == "additions") return 0;
    if (suffix == "context")   return 1;
    if (suffix == "deletions") return 2;
    if (suffix == "full_add")  return 3;
    if (suffix == "full_del")  return 4;
    return INT_MAX;
}

bool compareHunkPaths(const QString& leftPath,
                             const QString& rightPath,
                             const QRegularExpression& regex){

    QRegularExpressionMatch leftMatch  = regex.match(leftPath);
    QRegularExpressionMatch rightMatch = regex.match(rightPath);

    if (!leftMatch.hasMatch() || !rightMatch.hasMatch())
        return leftPath < rightPath;

    int leftHunkNumber  = leftMatch.captured(1).toInt();
    int rightHunkNumber = rightMatch.captured(1).toInt();

    if (leftHunkNumber != rightHunkNumber)
        return leftHunkNumber < rightHunkNumber;

    QString leftSuffix  = leftMatch.captured(2);
    QString rightSuffix = rightMatch.captured(2);

    int leftSuffixRank  = suffixRank(leftSuffix);
    int rightSuffixRank = suffixRank(rightSuffix);

    if (leftSuffixRank != rightSuffixRank)
        return leftSuffixRank < rightSuffixRank;

    return leftPath < rightPath;
}

void sortHunkFiles(QStringList& paths){
    QRegularExpression regex(R"(hunk_(\d+)_([a-z_]+)\.java)");

    std::stable_sort(
        paths.begin(),
        paths.end(),
        [&](const QString& a, const QString& b) -> bool {
            return compareHunkPaths(a, b, regex);
        }
    );
}
