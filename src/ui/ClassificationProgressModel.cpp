#include "headers/ui/ClassificationProgressModel.h"

ClassificationProgressModel::ClassificationProgressModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int ClassificationProgressModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

QVariant ClassificationProgressModel::data(const QModelIndex &index, int role) const
{
    if (index.row() < 0 || index.row() >= m_rows.size()) {
        return {};
    }

    const Row &row = m_rows.at(index.row());

    switch (role) {
    case PrNumberRole:      return row.prNumber;
    case CompletedFilesRole: return row.completedFiles;
    case TotalFilesRole:    return row.totalFiles;
    case StatusRole:        return row.status;
    case FinishedRole:      return row.finished;
    default:                return {};
    }
}

QHash<int, QByteArray> ClassificationProgressModel::roleNames() const
{
    return {
        {PrNumberRole, "prNumber"},
        {CompletedFilesRole, "completedFiles"},
        {TotalFilesRole, "totalFiles"},
        {StatusRole, "status"},
        {FinishedRole, "finished"}
    };
}

void ClassificationProgressModel::clear()
{
    if (m_rows.isEmpty()) {
        return;
    }

    beginResetModel();
    m_rows.clear();
    m_activeCount = 0;
    endResetModel();
}

void ClassificationProgressModel::setProgress(const QString &prNumber,
                                              int completedFiles,
                                              int totalFiles,
                                              const QString &status,
                                              bool finished)
{
    int row = indexOf(prNumber);

    if (row < 0) {
        appendRow(prNumber, completedFiles, totalFiles, status);
        row = m_activeCount - 1;
    } else {
        Row &existing = m_rows[row];
        existing.completedFiles = completedFiles;
        existing.totalFiles = totalFiles;
        existing.status = status;

        const QModelIndex changed = index(row);
        emit dataChanged(changed, changed,
                         {CompletedFilesRole, TotalFilesRole, StatusRole});
    }

    if (finished && !m_rows.at(row).finished) {
        moveRowToFinished(row);
    }
}

int ClassificationProgressModel::completedFilesFor(const QString &prNumber) const
{
    const int row = indexOf(prNumber);
    return row < 0 ? 0 : m_rows.at(row).completedFiles;
}

int ClassificationProgressModel::totalFilesFor(const QString &prNumber) const
{
    const int row = indexOf(prNumber);
    return row < 0 ? 0 : m_rows.at(row).totalFiles;
}

int ClassificationProgressModel::indexOf(const QString &prNumber) const
{
    for (int row = 0; row < m_rows.size(); ++row) {
        if (m_rows.at(row).prNumber == prNumber) {
            return row;
        }
    }
    return -1;
}

void ClassificationProgressModel::appendRow(const QString &prNumber,
                                            int completedFiles,
                                            int totalFiles,
                                            const QString &status)
{
    // New pull requests join the end of the active block, ahead of everything
    // that has already finished.
    beginInsertRows(QModelIndex(), m_activeCount, m_activeCount);
    m_rows.insert(m_activeCount, Row{prNumber, completedFiles, totalFiles,
                                     status, false});
    ++m_activeCount;
    endInsertRows();
}

void ClassificationProgressModel::moveRowToFinished(int row)
{
    const int destination = m_rows.size();

    // beginMoveRows wants the index the row lands in front of, which for a
    // move to the very end is one past the last row.
    if (row != m_rows.size() - 1) {
        beginMoveRows(QModelIndex(), row, row, QModelIndex(), destination);
        m_rows.move(row, m_rows.size() - 1);
        endMoveRows();
    }

    --m_activeCount;

    const QModelIndex changed = index(m_rows.size() - 1);
    m_rows.last().finished = true;
    emit dataChanged(changed, changed, {FinishedRole});
}
