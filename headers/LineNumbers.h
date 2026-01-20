#ifndef LINENUMBERS_H
#define LINENUMBERS_H

#include <QPlainTextEdit>
#include <QWidget>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QTextBlock>
#include <QRegularExpression>
#include "LineNumberHelpers.h"

class LineNumbers: public QWidget {
    public:
        LineNumbers(LineNumberHelpers* helper) : QWidget(helper), helpers(helper){}
        QSize sizeHint() const override{
            return {helpers->lineAreaWidth(), 0};
        }

    protected:
        void paintEvent(QPaintEvent* event) override{
            helpers->createLines(event);
        }
    private:
        LineNumberHelpers* helpers;
};

#endif // LINENUMBERS_H
