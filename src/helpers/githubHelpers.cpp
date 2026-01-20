#include "headers/helpers/githubHelpers.h"

QDateTime parseIsoGitHub(const QString& iso){
    // GitHub: "YYYY-MM-DDTHH:MM:SSZ"
    QDateTime dt = QDateTime::fromString(iso, Qt::ISODate);
    if (dt.isValid())
        dt.toTimeZone(QTimeZone::UTC);
    return dt;
}

bool get_response(const QString& url,
                  const QStringList& tokenList,
                  int& ct,
                  QJsonDocument& outDoc){
    outDoc = QJsonDocument();

    if (tokenList.isEmpty())
        return false;

    const int lenTokens = tokenList.size();
    ct = ct % lenTokens;

    QNetworkAccessManager manager;

    QNetworkRequest request{ QUrl(url) };
    request.setRawHeader("Authorization",
                         QByteArray("Bearer ") + tokenList.at(ct).toUtf8());
    request.setRawHeader("Accept", "application/vnd.github+json");

    QNetworkReply* reply = manager.get(request);

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    QByteArray responseData = reply->readAll();
    QNetworkReply::NetworkError err = reply->error();
    reply->deleteLater();

    ct++; // match token rotation

    if (err != QNetworkReply::NoError)
        return false;

    QJsonParseError parseErr;
    outDoc = QJsonDocument::fromJson(responseData, &parseErr);

    return (parseErr.error == QJsonParseError::NoError);
}

RepoDatesResult repo_dates(const QString& repo,
                           const QStringList& tokenList,
                           int& ct){
    RepoDatesResult res;

    // url = f'{constant.GITHUB_API_BASE_URL}{repo}'
    const QString url = GITHUB_API_BASE_URL + repo;

    QJsonDocument doc;
    if (!get_response(url, tokenList, ct, doc))
        return res;

    if (!doc.isObject())
        return res;

    const QJsonObject obj = doc.object();
    res.createdAt = obj.value("created_at").toString();
    res.updatedAt = obj.value("updated_at").toString();
    return res;
}

QString repo_commit_date(const QString& repo,
                         const QString& dateIso,
                         const QStringList& tokenList,
                         int& ct){
    // url = f'{BASE}{repo}/commits?until={date}'
    // Use QUrlQuery to properly encode
    QUrl url(GITHUB_API_BASE_URL + repo + "/commits");
    QUrlQuery q;
    q.addQueryItem("until", dateIso);
    url.setQuery(q);

    QJsonDocument doc;
    if (!get_response(url.toString(), tokenList, ct, doc))
        return QString();

    // Python expects an array: content_arrays[0]['sha']
    if (!doc.isArray())
        return QString();

    const QJsonArray arr = doc.array();
    if (arr.isEmpty() || !arr.at(0).isObject())
        return QString();

    const QJsonObject first = arr.at(0).toObject();
    return first.value("sha").toString();
}

DivergenceResult divergence_date_qt(const QString& mainline,
                                    const QString& variant,
                                    const QStringList& tokenList,
                                    int ct,
                                    QString leastDate,
                                    QString divergeDate){
    // created_ml, date_ml = repo_dates(mainline)
    // fork_date, date_vr  = repo_dates(variant)
    const RepoDatesResult ml = repo_dates(mainline, tokenList, ct);
    const RepoDatesResult vr = repo_dates(variant,  tokenList, ct);

    const QDateTime dateMl = parseIsoGitHub(ml.updatedAt);
    const QDateTime dateVr = parseIsoGitHub(vr.updatedAt);

    // If least_date == '':
    //   least_date = min(updated_at)
    if (leastDate.isEmpty()) {
        if (dateMl.isValid() && dateVr.isValid())
            leastDate = (dateMl < dateVr) ? ml.updatedAt : vr.updatedAt;
        else
            leastDate = !ml.updatedAt.isEmpty() ? ml.updatedAt : vr.updatedAt;
    }

    // sha_vr = repo_commit_date(variant, least_date)
    // sha_ml = repo_commit_date(mainline, least_date)
    const QString shaVr = repo_commit_date(variant,  leastDate, tokenList, ct);
    const QString shaMl = repo_commit_date(mainline, leastDate, tokenList, ct);

    // fork1 = variant.split('/')
    // url_ml = f'{BASE}{mainline}/compare/{sha_ml}...{fork1[0]}:{sha_vr}'
    const QString forkUser = variant.section('/', 0, 0);
    const QString compareUrl =
        QString("%1%2/compare/%3...%4:%5")
            .arg(GITHUB_API_BASE_URL, mainline, shaMl, forkUser, shaVr);

    QJsonDocument doc;
    const bool ok = get_response(compareUrl, tokenList, ct, doc);

    if (divergeDate.isEmpty()) {
        if (ok && doc.isObject()) {
            const QJsonObject obj = doc.object();

            // Python:
            // if 'commits' in content_arrays_ml:
            //   commits = content_arrays_ml['commits'][0]
            //   diverge_date = commits['commit']['committer']['date']
            if (obj.contains("commits") && obj.value("commits").isArray()) {
                const QJsonArray commits = obj.value("commits").toArray();
                if (!commits.isEmpty() && commits.at(0).isObject()) {
                    const QJsonObject c0 = commits.at(0).toObject();
                    const QJsonObject commitObj = c0.value("commit").toObject();
                    const QJsonObject committerObj = commitObj.value("committer").toObject();
                    divergeDate = committerObj.value("date").toString();
                }
            }

            // else: diverge_date = fork_date
            if (divergeDate.isEmpty())
                divergeDate = vr.createdAt;
        } else {
            // on error: diverge_date = fork_date
            divergeDate = vr.createdAt;
        }
    }

    return { divergeDate, leastDate };
}
