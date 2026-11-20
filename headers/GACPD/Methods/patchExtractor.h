#ifndef PATCHEXTRACTOR_H
#define PATCHEXTRACTOR_H

#include "../Structures/prPatches.h"
#include <QDateTime>
#include <QNetworkAccessManager>
#include <QThread>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QNetworkReply>
#include <stdexcept>
#include <functional>

QDateTime parseIsoZ(const QString &timestamp);
QString dateString(const QDateTime &dt);
QStringList uniqueStrings(const QStringList &items);

QString buildQuery(const QString &repo,
                    const QString &startDate,
                    const QString &endDate);

class GitHubGraphqlRequestError : public std::runtime_error {
public:
    GitHubGraphqlRequestError(int httpStatusCode,
                              QNetworkReply::NetworkError networkError,
                              const QString &responseBody,
                              const QString &message)
        : std::runtime_error(message.toStdString()),
          httpStatusCode(httpStatusCode),
          networkError(networkError),
          responseBody(responseBody) {}

    int httpStatusCode;
    QNetworkReply::NetworkError networkError;
    QString responseBody;
};

QJsonObject runQuery(QNetworkAccessManager &manager,
                    const QString &token,
                    const QString &query,
                    const QJsonObject &variables,
                    QJsonArray *attemptLog = nullptr);

QJsonObject runQueryWithTokenRotation(QNetworkAccessManager &manager,
                    const QStringList &tokenList,
                    int &tokenIndex,
                    const QString &query,
                    const QJsonObject &variables,
                    QJsonArray *attemptLog = nullptr);

void waitForReply(QNetworkReply *reply);

// Called after each page of merged PRs: the page just read, the most pages the
// scan can need (it stops early once PRs are created after the cutoff), and the
// bug-fix PRs matched so far.
using PrScanProgress = std::function<void(int page, int maxPages, int matches)>;

prPatches getPrPatches(const QString& repostitory,
                       const QString& divergenceDate,
                       const QString& leastDate,
                       const QStringList& tokenList,
                       int& ct,
                       const PrScanProgress& onPage = {});

QNetworkReply *sendGraphqlRequest(QNetworkAccessManager &manager,
                        const QString &token,
                        const QString &query,
                        const QJsonObject &variables);

windowSliceResult crawlSearchSlice(QNetworkAccessManager &manager,
                                const QString &repo,
                                const QDateTime &sliceStart,
                                const QDateTime &sliceEnd,
                                const QStringList &tokenList,
                                int& tokenIndex,
                                const QStringList &bugKeywords,
                                const QDateTime &globalStart,
                                const QDateTime &globalEnd,
                                bool printRows);

#endif // PATCHEXTRACTOR_H
