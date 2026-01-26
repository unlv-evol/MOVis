#include "headers/LineNumberHelpers.h"
#include "headers/LineNumbers.h"
#include <QFile>
#include <QPainter>

LineNumberHelpers::LineNumberHelpers(QWidget* parent):QPlainTextEdit(parent), lineWidget(new LineNumbers(this)){
    //QPlainTextEdit - stores text as blocks; paragraphs or lines
    // https://doc.qt.io/qt-6/qplaintextedit.html - More info on other options here
    setReadOnly(true);
    setTextInteractionFlags(Qt::TextSelectableByKeyboard |
                            Qt::TextSelectableByMouse |
                            Qt::LinksAccessibleByMouse); // https://doc.qt.io/qt-6/qt.html#TextInteractionFlag-enum
    setUndoRedoEnabled(false);
    setCursorWidth(0); // 0 -> hides the cursor
    viewport()->setCursor(Qt::ArrowCursor); // instead of "|" being used it'll be empty.
    setContextMenuPolicy(Qt::DefaultContextMenu); // Right Click options for copy and paste only

    // Used to ensure source code and log files is properly spaced in various OSs
    // As well as for the Widget
    QFont fontStyle = font();
    #if defined(Q_OS_WIN)
        fontStyle.setFamily(QStringLiteral("Consolas"));
    #else
        fontStyle.setFamily(QStringLiteral("Monospace"));
    #endif

    fontStyle.setStyleHint(QFont::Monospace);
    setFont(fontStyle);
    setWordWrapMode(QTextOption::NoWrap);
    setTabStopDistance(fontMetrics().horizontalAdvance(QLatin1Char(' ')) * 4);

    // Whenver a file is loaded send the # of lines to onBlockCountChanged
    // This includes: enter, delete, paste, undo/redo, load file
    connect(this, &QPlainTextEdit::blockCountChanged, this, &LineNumberHelpers::onBlockCountChanged);

    // Whenever the file is being scrolled, typed on, selected, text is added/removed, send the signal
    connect(this, &QPlainTextEdit::updateRequest, this, &LineNumberHelpers::onUpdateRequest);

    // Initializes the width/line total
    onBlockCountChanged(0);
}

void LineNumberHelpers::readFile(const QString &text) {
    // obtain start from potential line offsets
    updateStartLineNumber(text);

    // Ensures the UI is only updated once - to make it smooth
    const bool wasBlocked = blockSignals(true);
    setPlainText(text);
    blockSignals(wasBlocked);

    // Ensure FileLines width adapts to new line count
    onBlockCountChanged(0);
}

bool LineNumberHelpers::loadFile(const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    QTextStream fileContent(&file);

    // By defautl use UTF-8 Codec
    fileContent.setEncoding(QStringConverter::Utf8);

    readFile(fileContent.readAll());
    return true;
}

int LineNumberHelpers::getLineNumberStartOffset(int offset) const{
    return lineNumberStart + offset;
}

int LineNumberHelpers::lineAreaWidth() const{
    int digits = 1;
    int maxLine = 1;

    if (usePerBlockNumbers && !perBlockNumbers.isEmpty()) {
        for (int n : perBlockNumbers) {
            if (n > maxLine) maxLine = n;
        }
    } else {
        const int lineTotal = qMax(1, blockCount());
        maxLine = qMax(1, getLineNumberStartOffset(lineTotal - 1));
    }

    int tmp = maxLine;
    while (tmp >= 10){
        tmp /= 10;
        digits++;
    }
    // left-right padding total
    const int pad = 6;

    // # digits * 9 (since 9 is the widdest pixel) and the padding *2
    return pad + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits + pad;
}


void LineNumberHelpers::onBlockCountChanged(int useless){
    // parameter is required to follow QT's connection function.

    // we make enough space on the left of the widget for the line number.
    // If we add to the top, bottom and right of it, then we waste space. Leave at 0 (unless you want to add more stuff to it)
    setViewportMargins(lineAreaWidth(), 0, 0, 0);
}

void LineNumberHelpers::onUpdateRequest(const QRect &rect, int scrollY){
    // Updates the viewing based on scroll
    // Negative value means it moved up
    // Positive means it moved down
    if(scrollY != 0){
        lineWidget->scroll(0, scrollY);
    }
    else{
        // This else is here for when a selection is done, or any other form udpate taht is not scrolling.
        // Removing it causes some unexpected issues
        lineWidget->update(0, rect.y(), lineWidget->width(), rect.height());
    }

    // This is here for when the user maximizes the screen, so things properly match the new size
    if (rect.contains(viewport()->rect()))
        onBlockCountChanged(0);
}

void LineNumberHelpers::resizeEvent(QResizeEvent *event) {
    QPlainTextEdit::resizeEvent(event); // call the default resize event
    const QRect contents = contentsRect(); // gives us the dimensions of the new area

    // sets the widget for the code lines to fit within those new dimensions
    lineWidget->setGeometry(QRect(contents.left(), contents.top(), lineAreaWidth(), contents.height()));
}

void LineNumberHelpers::createLines(QPaintEvent *event){
    /*
     * Usefull links:
     * https://doc.qt.io/qt-6/qpainter.html#fillRect
     * https://doc.qt.io/qt-6/qpainter.html#drawLine
     * https://doc.qt.io/qt-6/qpalette.html
     * https://doc.qt.io/qt-6/qpalette.html#ColorGroup-enum
     * https://doc.qt.io/qt-6/qplaintextedit.html#blockBoundingGeometry
     * https://doc.qt.io/qt-6/qplaintextedit.html#contentOffset
     */

    QPainter painter(lineWidget);

    //Background: use the editor/base background from the current theme
    const QColor bg = palette().color(QPalette::Base);
    painter.fillRect(event->rect(), bg);

    // Separator derive from theme mid/dark (good contrast in both light/dark)
    QColor sep = palette().color(QPalette::Mid);
    if (!sep.isValid()) sep = palette().color(QPalette::Dark);
    painter.setPen(sep);
    painter.drawLine(lineWidget->width() - 1,
                     event->rect().top(),
                     lineWidget->width() - 1,
                     event->rect().bottom());

    // Line number text color
    QColor numColor = palette().color(QPalette::Text);

    // Optional: slightly de-emphasize line numbers but keep readable
    numColor.setAlpha(200);   // tweak: 160-230 range is usually good

    // We use firstVisibleBlock to get an accurate line to showcase (based on current view)
    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();

    // Get line number based on current view
    int topNumber = (int) blockBoundingGeometry(block).translated(contentOffset()).top();

    // Gets the lowerbound based on current view
    int bottomNumber = topNumber + (int) blockBoundingRect(block).height();

    // While the block exists and while its inside the visible area.
    while(block.isValid() && topNumber<=event->rect().bottom()){
        if(block.isVisible() && bottomNumber>=event->rect().top()){
            int displayNum = 0;

            if (usePerBlockNumbers) {
                if (blockNumber >= 0 && blockNumber < perBlockNumbers.size())
                    displayNum = perBlockNumbers[blockNumber];
            } else {
                displayNum = getLineNumberStartOffset(blockNumber);
            }

            if (displayNum > 0) {
            // Set the number based on offset
            QString number = QString::number(getLineNumberStartOffset(displayNum));
            painter.setPen(numColor);

            // Vertically and Right-Aligned Text Drawing
            painter.drawText(0,
                             topNumber,
                             lineWidget->width()-6,
                             fontMetrics().height(),
                             Qt::AlignRight | Qt::AlignVCenter,
                             number);
            }
        }

        // No matter what - go to the next block
        block = block.next();
        topNumber = bottomNumber;

        // Update pixel bounds
        bottomNumber = topNumber + (int)blockBoundingGeometry(block).height();

        // Update index
        blockNumber++;
    }
}

void LineNumberHelpers::updateStartLineNumber(const QString& text){
    if(overrideLineNumbers == false){
        lineNumberStart = 0;
        return;
    }

    // Find Line changes in hunk
    // @@ -55,8 +53,6 @@
    const QRegularExpression hunkLinesRegex(
        R"(^\s*@@\s+-\d+(?:,\d+)?\s+\+(\d+)(?:,\d+)?\s+@@)",
        QRegularExpression::MultilineOption
    );

    const QRegularExpressionMatch match = hunkLinesRegex.match(text);
    if(match.hasMatch()){
        bool success = false;
        const int start = match.captured(1).toInt(&success);    // The first captured from the regex.
        if(success){
            lineNumberStart = qMax(1, start);
        }else{
            lineNumberStart = 1;
        }
    }else{ // Fallback incase of errors
        lineNumberStart = 1;
    }
}

void LineNumberHelpers::setOverrideLineNumbers(bool enabled){
    overrideLineNumbers = enabled;
}
void LineNumberHelpers::setLineNumberStart(int startNumber){
    lineNumberStart = startNumber;
}

void LineNumberHelpers::setPerBlockLineNumbers(const QVector<int>& numbers)
{
    perBlockNumbers = numbers;
    usePerBlockNumbers = true;
    onBlockCountChanged(0); // refresh margin width
    viewport()->update();
}

void LineNumberHelpers::clearPerBlockLineNumbers()
{
    perBlockNumbers.clear();
    usePerBlockNumbers = false;
    onBlockCountChanged(0);
    viewport()->update();
}
