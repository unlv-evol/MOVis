#ifndef SELECTION_MODEL_H
#define SELECTION_MODEL_H

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>

// Backing model for one combo box in the results viewer.
//
// Each entry pairs the label QML shows with the value the viewer needs when
// that entry is picked -- usually a file path. Keeping the list here rather
// than in QML lets the comparison logic change the selection itself (for
// example when a PR search jumps to another classification) and lets it read
// the selected path back without asking the view for it.
class SelectionModel final : public QObject
{
    Q_OBJECT

public:
    explicit SelectionModel(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    void addItem(const QString &label, const QVariant &value = QVariant())
    {
        m_items.append({label, value});
        emit changed();
    }

    void addItems(const QStringList &labels)
    {
        for (const QString &label : labels) {
            m_items.append({label, {}});
        }
        emit changed();
    }

    void clear()
    {
        m_items.clear();
        m_currentIndex = -1;
        emit changed();
    }

    int count() const { return m_items.size(); }
    int currentIndex() const { return m_currentIndex; }

    QString currentText() const
    {
        return isValidIndex(m_currentIndex)
            ? m_items.at(m_currentIndex).label
            : QString();
    }

    QVariant currentValue() const { return valueAt(m_currentIndex); }

    QVariant valueAt(int index) const
    {
        return isValidIndex(index) ? m_items.at(index).value : QVariant();
    }

    // Index of the entry carrying `value`, or -1 when there is none.
    int indexOfValue(const QVariant &value) const
    {
        for (int index = 0; index < m_items.size(); ++index) {
            if (m_items.at(index).value == value) {
                return index;
            }
        }
        return -1;
    }

    int indexOfLabel(const QString &label) const
    {
        for (int index = 0; index < m_items.size(); ++index) {
            if (m_items.at(index).label == label) {
                return index;
            }
        }
        return -1;
    }

    // Out-of-range indexes clear the selection instead of being rejected, so
    // callers can pass the result of a lookup straight through.
    void setCurrentIndex(int index)
    {
        const int normalized = isValidIndex(index) ? index : -1;
        if (m_currentIndex == normalized) {
            return;
        }

        m_currentIndex = normalized;
        emit changed();
        emit currentIndexChanged(m_currentIndex);
    }

    bool isEnabled() const { return m_enabled; }

    void setEnabled(bool enabled)
    {
        if (m_enabled == enabled) {
            return;
        }
        m_enabled = enabled;
        emit changed();
    }

    QStringList labels() const
    {
        QStringList result;
        result.reserve(m_items.size());
        for (const Item &item : m_items) {
            result.append(item.label);
        }
        return result;
    }

signals:
    // Anything that changes what QML should draw: the list, the selection or
    // the enabled state.
    void changed();

    // Only the selection, so the viewer can reload the panes without also
    // reacting to its own list rebuilds.
    void currentIndexChanged(int index);

private:
    struct Item {
        QString label;
        QVariant value;
    };

    bool isValidIndex(int index) const
    {
        return index >= 0 && index < m_items.size();
    }

    QList<Item> m_items;
    int m_currentIndex = -1;
    bool m_enabled = true;
};

#endif // SELECTION_MODEL_H
