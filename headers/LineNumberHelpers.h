#ifndef LINENUMBERHELPERS_H
#define LINENUMBERHELPERS_H

#include <QPlainTextEdit>
#include <QWidget>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QTextBlock>
#include <QRegularExpression>
#include <QVector>

class LineNumberHelpers: public QPlainTextEdit {
    Q_OBJECT
public:
    LineNumberHelpers(QWidget *parent = nullptr);

    // Function that sets the content of the file
    void readFile(const QString &text);

    // Function that loads the file
    bool loadFile(const QString &filePath);

    // Simple getter function
    int lineAreaWidth() const;

    // Function to create the line Numbers
    void createLines(QPaintEvent *event);

    // Simple setter function
    void setOverrideLineNumbers(bool enabled);
    void setLineNumberStart(int startNumber);

    void setPerBlockLineNumbers(const QVector<int>& numbers);
    void clearPerBlockLineNumbers();

protected:
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onBlockCountChanged(int);
    void onUpdateRequest(const QRect &rect, int scrollY);

private:
    QWidget* lineWidget;
    bool overrideLineNumbers = false;
    int lineNumberStart = -1;

    bool usePerBlockNumbers = false;
    QVector<int> perBlockNumbers;

    // updates the startLineNumber
    void updateStartLineNumber(const QString &text);

    // Obtains the startLineNumber with line offset
    int getLineNumberStartOffset(int lineNumberOffset) const;
};

#endif // LINENUMBERHELPERS_H
