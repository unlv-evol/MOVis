#ifndef CLASSIFICATION_PROGRESS_MODEL_H
#define CLASSIFICATION_PROGRESS_MODEL_H

#include <QAbstractListModel>
#include <QHash>
#include <QList>
#include <QString>

// One row per pull request in the "Running GACPD Classification" dialog.
//
// This is a real list model rather than a list of maps so that a file-count
// update touches only the row that changed. Handing QML a fresh list would
// make it throw away and rebuild every delegate, which shows up as the whole
// dialog flickering while a run is in progress.
//
// Rows are kept in two blocks: the pull requests still being worked on, in the
// order they were queued, followed by the ones that have finished.
class ClassificationProgressModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        PrNumberRole = Qt::UserRole + 1,
        CompletedFilesRole,
        TotalFilesRole,
        StatusRole,
        FinishedRole
    };

    explicit ClassificationProgressModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Drops every row. Called when a new run starts.
    void clear();

    // Adds the pull request or updates the row it already has. A row that
    // becomes finished moves to the bottom of the list.
    void setProgress(const QString &prNumber,
                     int completedFiles,
                     int totalFiles,
                     const QString &status,
                     bool finished);

    // Current counts for a pull request, so callers can update one field
    // without having to remember the others.
    int completedFilesFor(const QString &prNumber) const;
    int totalFilesFor(const QString &prNumber) const;

    bool isEmpty() const { return m_rows.isEmpty(); }
    bool contains(const QString &prNumber) const { return indexOf(prNumber) >= 0; }

private:
    struct Row {
        QString prNumber;
        int completedFiles = 0;
        int totalFiles = 0;
        QString status = QStringLiteral("waiting");
        bool finished = false;
    };

    int indexOf(const QString &prNumber) const;
    void appendRow(const QString &prNumber, int completedFiles, int totalFiles,
                   const QString &status);
    void moveRowToFinished(int row);

    QList<Row> m_rows;

    // Rows [0, m_activeCount) are still running; the rest have finished.
    int m_activeCount = 0;
};

#endif // CLASSIFICATION_PROGRESS_MODEL_H
