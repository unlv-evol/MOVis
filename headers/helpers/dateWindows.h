#ifndef DATE_WINDOWS_H
#define DATE_WINDOWS_H

#include <QDate>
#include <QDateTime>
#include <QList>
#include <QString>

// A classification run covers at most six months of pull requests, so a longer
// range is handled as a series of consecutive windows.
struct DateWindow {
    QDate start;
    QDate end;
};

// Splits [from, to] into gap-free six-month windows. Returns an empty list if
// either bound is invalid or the range runs backwards.
QList<DateWindow> splitIntoSixMonthWindows(const QDateTime &from,
                                           const QDateTime &to);

// GitHub's search API wants whole days, so a window is sent as the very start
// of its first day through the very end of its last.
QString isoStartOfDay(const QDate &date);
QString isoEndOfDay(const QDate &date);

#endif // DATE_WINDOWS_H
