#include "headers/results/CodeDocument.h"

#include <QColor>
#include <QFont>
#include <QMap>
#include <QTextBlock>
#include <QVariantMap>

CodeDocument::CodeDocument(QObject *parent)
    : QObject(parent)
    , m_cursor(&m_document)
{
    // toHtml() writes the document's default font into the <body> style, and
    // that overrides whatever font the Qt Quick pane sets. Match Theme's
    // monoFontFamily and fontSize so the code and its line-number gutter
    // share one line height.
#ifdef Q_OS_WIN
    QFont font(QStringLiteral("Consolas"));
#else
    QFont font(QStringLiteral("Menlo"));
#endif
    font.setStyleHint(QFont::Monospace);
    font.setPixelSize(13);
    m_document.setDefaultFont(font);
}

QString CodeDocument::lineNumbersText() const
{
    QStringList numbers;
    const int blockCount = m_document.blockCount();
    numbers.reserve(blockCount);

    for (int block = 0; block < blockCount; ++block) {
        int number = m_lineNumberStart + block;
        if (m_overrideLineNumbers && block < m_perBlockLineNumbers.size()) {
            number = m_perBlockLineNumbers.at(block);
        }
        numbers.append(number > 0 ? QString::number(number) : QString());
    }

    return numbers.join('\n');
}

static void appendBand(QVariantList &bands, int start, int end, const QColor &color)
{
    if (start < 0) {
        return;
    }
    bands.append(QVariantMap{
        {QStringLiteral("line"), start},
        {QStringLiteral("count"), end - start + 1},
        {QStringLiteral("color"), color},
    });
}

QVariantList CodeDocument::lineBands() const
{
    // Later selections are merged over earlier ones, so the last colour set
    // on a line is the one that shows.
    QMap<int, QColor> colorByLine;
    for (const TextExtraSelection &selection : m_selections) {
        if (!selection.format.property(QTextFormat::FullWidthSelection).toBool()
            || !selection.format.hasProperty(QTextFormat::BackgroundBrush)) {
            continue;
        }
        const QTextBlock first = m_document.findBlock(selection.cursor.selectionStart());
        const QTextBlock last = m_document.findBlock(selection.cursor.selectionEnd());
        if (!first.isValid() || !last.isValid()) {
            continue;
        }
        const QColor color = selection.format.background().color();
        for (int line = first.blockNumber(); line <= last.blockNumber(); ++line) {
            colorByLine.insert(line, color);
        }
    }

    QVariantList bands;
    int runStart = -1;
    int runEnd = -1;
    QColor runColor;
    for (QMap<int, QColor>::const_iterator it = colorByLine.cbegin(); it != colorByLine.cend(); ++it) {
        if (runStart >= 0 && it.key() == runEnd + 1 && it.value() == runColor) {
            runEnd = it.key();
            continue;
        }
        appendBand(bands, runStart, runEnd, runColor);
        runStart = runEnd = it.key();
        runColor = it.value();
    }
    appendBand(bands, runStart, runEnd, runColor);

    return bands;
}

void CodeDocument::setTextCursor(const QTextCursor &cursor)
{
    m_cursor = cursor;
    emit cursorPositionChanged();
    emit navigationRequested(m_cursor.selectionStart(), m_cursor.selectionEnd());
}

void CodeDocument::clear()
{
    setPlainText({});
    clearPerBlockLineNumbers();
}

void CodeDocument::setPlainText(const QString &text)
{
    m_document.setPlainText(text);
    m_cursor = QTextCursor(&m_document);
    m_selections.clear();
    emit contentChanged();
}

void CodeDocument::resetCharacterFormatting()
{
    QTextCursor cursor(&m_document);
    cursor.select(QTextCursor::Document);
    cursor.setCharFormat(QTextCharFormat());
}

void CodeDocument::setExtraSelections(
    const QList<TextExtraSelection> &selections)
{
    resetCharacterFormatting();
    m_selections = selections;

    for (const TextExtraSelection &selection : selections) {
        QTextCursor cursor = selection.cursor;
        cursor.mergeCharFormat(selection.format);
    }

    emit contentChanged();
}

void CodeDocument::setOverrideLineNumbers(bool enabled)
{
    if (m_overrideLineNumbers == enabled) {
        return;
    }

    m_overrideLineNumbers = enabled;
    emit contentChanged();
}

void CodeDocument::setLineNumberStart(int start)
{
    const int normalizedStart = qMax(1, start);
    if (m_lineNumberStart == normalizedStart) {
        return;
    }

    m_lineNumberStart = normalizedStart;
    emit contentChanged();
}

void CodeDocument::setPerBlockLineNumbers(
    const QVector<int> &lineNumbers)
{
    m_perBlockLineNumbers = lineNumbers;
    emit contentChanged();
}

void CodeDocument::clearPerBlockLineNumbers()
{
    if (m_perBlockLineNumbers.isEmpty()) {
        return;
    }
    m_perBlockLineNumbers.clear();
    emit contentChanged();
}

void CodeDocument::moveCursorTo(int position)
{
    QTextCursor cursor(&m_document);
    cursor.setPosition(qBound(0, position, m_document.characterCount() - 1));
    m_cursor = cursor;
    emit cursorPositionChanged();
}
