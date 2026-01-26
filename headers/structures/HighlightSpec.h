#ifndef HIGHLIGHTSPEC_H
#define HIGHLIGHTSPEC_H

#include <QColor>

enum Pane { LeftPane, RightPane };

struct HighlightSpec {
    Pane pane;
    int startLine;
    int startColumn;
    int endLine;
    int endColumn;
    QColor color;
};


#endif // HIGHLIGHTSPEC_H
