#include "headers/helpers/dateWindows.h"

QList<DateWindow> splitIntoSixMonthWindows(const QDateTime &from,
                                           const QDateTime &to)
{
    QList<DateWindow> windows;

    if (!from.isValid() || !to.isValid()) {
        return windows;
    }

    QDate currentStart = from.date();
    const QDate finalDate = to.date();

    if (currentStart > finalDate) {
        return windows;
    }

    while (currentStart <= finalDate) {
        // Work strictly with calendar dates. The previous addMonths(6)
        // + addSecs(-1) approach was later converted back to a date and then
        // expanded to 23:59:59, which could accidentally include the first
        // day of month seven. This produces exact, gap-free six-month slices.
        QDate currentEnd = currentStart.addMonths(6).addDays(-1);

        if (currentEnd > finalDate) {
            currentEnd = finalDate;
        }

        windows.append({currentStart, currentEnd});
        currentStart = currentEnd.addDays(1);
    }

    return windows;
}

QString isoStartOfDay(const QDate &date)
{
    return date.toString(Qt::ISODate) + "T00:00:00Z";
}

QString isoEndOfDay(const QDate &date)
{
    return date.toString(Qt::ISODate) + "T23:59:59Z";
}
