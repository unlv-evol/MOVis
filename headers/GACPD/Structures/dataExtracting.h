#ifndef DATAEXTRACTING_H
#define DATAEXTRACTING_H

#include <QMap>

struct prDataExtractor {
    QMap<QString, QVariantMap> pullRequestData;
    int requestCount = 0;
    double runtimeSeconds = 0.0;
};

struct VariantShaExtractor {
    QString variantSha;
};

#endif // DATAEXTRACTING_H
