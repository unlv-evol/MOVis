#ifndef CODE_DOCUMENT_H
#define CODE_DOCUMENT_H

#include <QList>
#include <QObject>
#include <QStringList>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QVariantList>
#include <QVector>

struct TextExtraSelection {
    QTextCursor cursor;
    QTextCharFormat format;
};

// A presentation-neutral text document used by the Qt Quick results viewer.
// It retains the old viewer's document, cursor, line-number and highlighting
// behavior without requiring a QPlainTextEdit.
class CodeDocument final : public QObject
{
    Q_OBJECT

public:
    explicit CodeDocument(QObject *parent = nullptr);

    QTextDocument *document() { return &m_document; }
    const QTextDocument *document() const { return &m_document; }

    QString toPlainText() const { return m_document.toPlainText(); }
    QString toHtml() const { return m_document.toHtml(); }
    QString lineNumbersText() const;

    // Runs of consecutive lines that carry the same full-width highlight, as
    // { line, count, color } maps. The Qt Quick pane paints these behind the
    // text so a highlight spans the whole line, as QPlainTextEdit's
    // FullWidthSelection did, rather than stopping where the text ends.
    QVariantList lineBands() const;

    QTextCursor textCursor() const { return m_cursor; }
    void setTextCursor(const QTextCursor &cursor);

    void clear();
    void setPlainText(const QString &text);
    void setExtraSelections(const QList<TextExtraSelection> &selections);

    void setOverrideLineNumbers(bool enabled);
    void setLineNumberStart(int start);
    void setPerBlockLineNumbers(const QVector<int> &lineNumbers);
    void clearPerBlockLineNumbers();

    Q_INVOKABLE void moveCursorTo(int position);

signals:
    void contentChanged();
    void cursorPositionChanged();
    void navigationRequested(int selectionStart, int selectionEnd);

private:
    void resetCharacterFormatting();

    QTextDocument m_document;
    QTextCursor m_cursor;
    QList<TextExtraSelection> m_selections;
    QVector<int> m_perBlockLineNumbers;
    bool m_overrideLineNumbers = false;
    int m_lineNumberStart = 1;
};

#endif // CODE_DOCUMENT_H
