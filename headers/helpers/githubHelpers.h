#ifndef GITHUBHELPERS_H
#define GITHUBHELPERS_H

#include <QString>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QEventLoop>
#include <QUrlQuery>
#include <QTimeZone>

const QString GITHUB_API_BASE_URL = "https://api.github.com/repos/";

struct RepoDatesResult {
    QString createdAt;
    QString updatedAt;
};

struct DivergenceResult {
    QString divergeDate; // divergence_date
    QString leastDate;   // least_date
};

QDateTime parseIsoGitHub(const QString& iso);
bool get_response(const QString& url,
                  const QStringList& tokenList,
                  int& ct,
                  QJsonDocument& outDoc);
RepoDatesResult repo_dates(const QString& repo,
                           const QStringList& tokenList,
                           int& ct);

QString repo_commit_date(const QString& repo,
                         const QString& dateIso,
                         const QStringList& tokenList,
                         int& ct);

DivergenceResult divergence_date_qt(const QString& mainline,
                                    const QString& variant,
                                    const QStringList& tokenList,
                                    int ct,
                                    QString leastDate = QString(),
                                    QString divergeDate = QString());

#endif // GITHUBHELPERS_H
