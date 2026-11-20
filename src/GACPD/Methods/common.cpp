#include "headers/GACPD/Methods/common.h"
#include <QCryptographicHash>
#include <QMutex>
#include <QMutexLocker>
#include <QSet>
#include <QThread>
#include <stdexcept>

void rotateToken(int& ct, const QStringList& tokenList){
    #ifdef QT_DEBUG
        qDebug() << "Previous token index:" << (ct + 1);
    #endif
    if(ct+1 >= tokenList.size()){
        ct = 0;
    }
    else{
        ct++;
    }
}

static QMutex restTokenMutex;
static QSet<QByteArray> rejectedRestTokens;

static QString cleanToken(QString token) {
    token = token.trimmed();
    if (token.startsWith("Bearer ", Qt::CaseInsensitive)) token = token.mid(7).trimmed();
    else if (token.startsWith("token ", Qt::CaseInsensitive)) token = token.mid(6).trimmed();
    return token;
}

QJsonValue githubRestRequest(const QString &url, const QStringList &tokens, int &ct,
                             bool allowNotFound, QJsonArray *attemptLog, int *requestCount) {
    if (tokens.isEmpty()) throw std::runtime_error("No GitHub tokens configured");
    QNetworkAccessManager manager;
    QString lastFailure = "No usable GitHub tokens remain";
    for (qsizetype tokenTry = 0; tokenTry < tokens.size(); ++tokenTry) {
        const int index = ((ct % tokens.size()) + tokens.size()) % tokens.size();
        ct = (index + 1) % tokens.size();
        const QString token = cleanToken(tokens[index]);
        const QByteArray fingerprint = QCryptographicHash::hash(token.toUtf8(), QCryptographicHash::Sha256);
        {
            QMutexLocker locker(&restTokenMutex);
            if (token.isEmpty() || rejectedRestTokens.contains(fingerprint)) continue;
        }
        for (int attempt = 0; attempt < 3; ++attempt) {
            QNetworkRequest request{QUrl(url)};
            request.setTransferTimeout(30000);
            request.setRawHeader("Authorization", ("Bearer " + token).toUtf8());
            request.setRawHeader("Accept", "application/vnd.github+json");
            request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
            request.setRawHeader("User-Agent", "MOVis");
            request.setRawHeader("Cache-Control", "no-cache");
            QNetworkReply *reply = manager.get(request);
            if (requestCount) ++*requestCount;
            QEventLoop loop;
            QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
            loop.exec();
            int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QNetworkReply::NetworkError networkError = reply->error();
            const QString networkErrorString = reply->errorString();
            // Over HTTP/2 Qt can finish a 401 reply with AuthenticationRequiredError
            // ("Host requires authentication") before the status code attribute is
            // populated, leaving status == 0. Treat it as the 401 it is, so the
            // token is rejected and the next one is tried instead of failing.
            if (status == 0 && networkError == QNetworkReply::AuthenticationRequiredError)
                status = 401;
            const QByteArray body = reply->readAll();
            reply->deleteLater();
            QJsonParseError parseError;
            const QJsonDocument doc = QJsonDocument::fromJson(body, &parseError);
            if (attemptLog) {
                QJsonObject entry;
                entry["attempt"] = attemptLog->size() + 1;
                entry["url"] = url;
                entry["status_code"] = status;
                entry["token_index"] = index + 1;
                if (doc.isObject()) entry["response"] = doc.object();
                else if (doc.isArray()) entry["response"] = doc.array();
                attemptLog->append(entry);
            }
            if (status == 200 && networkError == QNetworkReply::NoError &&
                parseError.error == QJsonParseError::NoError) {
                if (doc.isObject()) return doc.object();
                if (doc.isArray()) return doc.array();
            }
            if (status == 404 && allowNotFound) return QJsonValue(QJsonValue::Null);
            lastFailure = QString("GitHub request failed (HTTP %1): %2").arg(status).arg(url);
            if (status == 0) // No HTTP response: say why (timeout, DNS, TLS, ...).
                lastFailure += QString(" [network error: %1]").arg(networkErrorString);
            if (status == 401) {
                bool newlyRejected = false;
                {
                    QMutexLocker locker(&restTokenMutex);
                    newlyRejected = !rejectedRestTokens.contains(fingerprint);
                    rejectedRestTokens.insert(fingerprint);
                }
                // Qt also logs "qt.network.http2: ... Host requires authentication"
                // for this reply. Say which token it was so it can be removed;
                // it is skipped for the rest of this run either way.
                if (newlyRejected) {
                    qWarning().noquote()
                        << QString("GitHub rejected token %1 of %2 (HTTP 401: expired or "
                                   "revoked). Skipping it for the rest of this run.")
                               .arg(index + 1).arg(tokens.size());
                }
                break;
            }
            if (status == 403 || status == 429) break; // Try another configured token.
            if (status != 0 && status != 200 && status < 500)
                throw std::runtime_error(lastFailure.toStdString());
            if (attempt == 2) throw std::runtime_error(lastFailure.toStdString());
            QThread::msleep(200 * (1 << attempt));
        }
    }
    throw std::runtime_error(lastFailure.toStdString());
}

QJsonValue apiRequest(const QString &url, const QString &token) {
    int ct = 0;
    try {
        return githubRestRequest(url, QStringList{token}, ct);
    } catch (const std::exception &error) {
        qCritical() << error.what();
        return QJsonValue(QJsonValue::Undefined);
    }
}
