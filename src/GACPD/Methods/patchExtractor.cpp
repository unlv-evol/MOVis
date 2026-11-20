#include "headers/GACPD/Methods/patchExtractor.h"
#include "headers/GACPD/Methods/helpers_GACPD.h"
#include "headers/GACPD/Methods/common.h"
#include <QCryptographicHash>
#include <QMutex>
#include <QMutexLocker>

const QString API_URL = "https://api.github.com/graphql";
QSet<QString> prSet;

static QMutex rejectedTokenMutex;
static QSet<QByteArray> rejectedTokenFingerprints;

static QString normalizeGitHubToken(QString token){
    token = token.trimmed();

    if (token.startsWith("Bearer ", Qt::CaseInsensitive)) {
        token = token.mid(7).trimmed();
    } else if (token.startsWith("token ", Qt::CaseInsensitive)) {
        token = token.mid(6).trimmed();
    }

    return token;
}

static QByteArray tokenFingerprint(const QString &token){
    return QCryptographicHash::hash(token.toUtf8(),
                                    QCryptographicHash::Sha256);
}

static bool isRejectedToken(const QString &token){
    QMutexLocker locker(&rejectedTokenMutex);
    return rejectedTokenFingerprints.contains(tokenFingerprint(token));
}

static void rejectToken(const QString &token){
    QMutexLocker locker(&rejectedTokenMutex);
    rejectedTokenFingerprints.insert(tokenFingerprint(token));
}

static bool isAuthenticationFailure(const GitHubGraphqlRequestError &error){
    if (error.httpStatusCode == 401 ||
        error.networkError == QNetworkReply::AuthenticationRequiredError) {
        return true;
    }

    return error.responseBody.contains("bad credentials",
                                       Qt::CaseInsensitive);
}

QDateTime parseIsoZ(const QString &timestamp){
    QDateTime dt = QDateTime::fromString(timestamp, Qt::ISODate);

    if (!dt.isValid()) {
        throw std::runtime_error(
            QString("Invalid timestamp. Expected YYYY-MM-DDTHH:MM:SSZ, got: %1")
                .arg(timestamp)
                .toStdString()
            );
    }

    return dt.toUTC();
}

QString dateString(const QDateTime &dt){
    return dt.date().toString(Qt::ISODate);
}

QStringList uniqueStrings(const QStringList &items){
    QStringList result;
    QSet<QString> seen;

    for (const QString &item : items) {
        if (!seen.contains(item)) {
            seen.insert(item);
            result.append(item);
        }
    }

    return result;
}

QString buildQuery(const QString &repo,
    const QString &startDate,
    const QString &endDate){

    return QString(R"GRAPHQL(
        query($after: String) {
          search(
            query: "repo:%1 is:pr is:merged merged:%2..%3",
            type: ISSUE,
            first: 100,
            after: $after
          ) {
            issueCount
            nodes {
              ... on PullRequest {
                title
                number
                createdAt
                mergedAt
              }
            }
            pageInfo {
              hasNextPage
              endCursor
            }
          }
        }
        )GRAPHQL")
        .arg(repo, startDate, endDate);
}

QNetworkReply *sendGraphqlRequest(QNetworkAccessManager &manager,
    const QString &token,
    const QString &query,
    const QJsonObject &variables){

    // Build the HTTP request for GitHub's GraphQL endpoint.
    QNetworkRequest request{QUrl(API_URL)};

    const QString normalizedToken = normalizeGitHubToken(token);

    // GitHub GraphQL requires a Bearer token. Keep the token itself out of
    // logs and normalize accidental "Bearer "/"token " prefixes.
    request.setRawHeader("Authorization",
                        QString("Bearer %1").arg(normalizedToken).toUtf8());
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
    request.setRawHeader("User-Agent", "MOVis/0.17");

    // Tell GitHub that the body is JSON.
    request.setHeader(
        QNetworkRequest::ContentTypeHeader,
        "application/json"
        );

    // Build the JSON payload:
    //
    // {
    //   "query": "...",
    //   "variables": { ... }
    // }
    QJsonObject payload;
    payload["query"] = query;
    payload["variables"] = variables;

    // Convert the JSON object into compact JSON bytes and POST it.
    return manager.post(request,
                        QJsonDocument(payload).toJson(QJsonDocument::Compact));
}

void waitForReply(QNetworkReply *reply)
{
    // QNetworkAccessManager is asynchronous by default.
    // This local event loop blocks until this specific reply finishes.
    QEventLoop loop;

    QObject::connect(reply,
        &QNetworkReply::finished,
        &loop,
        &QEventLoop::quit);

    loop.exec();
}

static void appendAttempt(QJsonArray *attemptLog,
                          int attempt,
                          int statusCode,
                          const QByteArray &responseBody,
                          QNetworkReply::NetworkError networkError,
                          const QString &errorString){
    if (attemptLog == nullptr) {
        return;
    }

    QJsonObject entry;
    entry["attempt"] = attempt;
    entry["status_code"] = statusCode;

    QJsonParseError parseError;
    const QJsonDocument responseDoc =
        QJsonDocument::fromJson(responseBody, &parseError);

    if (parseError.error == QJsonParseError::NoError) {
        if (responseDoc.isObject()) {
            entry["response"] = responseDoc.object();
        } else if (responseDoc.isArray()) {
            entry["response"] = responseDoc.array();
        } else {
            entry["response"] = QJsonValue(QJsonValue::Null);
        }
    } else {
        entry["response_body"] = QString::fromUtf8(responseBody);
    }

    if (networkError != QNetworkReply::NoError) {
        entry["network_error"] = errorString;
    }

    attemptLog->append(entry);
}

QJsonObject runQuery(QNetworkAccessManager &manager,
    const QString &token,
    const QString &query,
    const QJsonObject &variables,
    QJsonArray *attemptLog){
    // Send the first GraphQL request.
    QNetworkReply *reply = sendGraphqlRequest(manager,
                                                token,
                                                query,
                                                variables);

    // Wait until the network request completes.
    waitForReply(reply);

    // Read the HTTP status code, response body, and Qt network state.
    int statusCode = reply
                         ->attribute(QNetworkRequest::HttpStatusCodeAttribute)
                         .toInt();

    QByteArray responseBody = reply->readAll();
    QNetworkReply::NetworkError networkError = reply->error();
    QString errorString = reply->errorString();

    appendAttempt(attemptLog, 1, statusCode, responseBody, networkError, errorString);

    // Schedule the reply object for cleanup.
    reply->deleteLater();

    // Retry once if GitHub returns a server-side error.
    if (statusCode >= 500) {
        QThread::msleep(1200);

        reply = sendGraphqlRequest(manager,
                                    token,
                                    query,
                                    variables);

        waitForReply(reply);

        statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        responseBody = reply->readAll();
        networkError = reply->error();
        errorString = reply->errorString();

        appendAttempt(attemptLog, 2, statusCode, responseBody, networkError, errorString);

        reply->deleteLater();
    }

    // Handle HTTP errors before the generic Qt network error. Qt maps HTTP
    // 401 to AuthenticationRequiredError ("Host requires authentication").
    // Reporting the HTTP response first preserves GitHub's useful JSON body,
    // e.g. {"message":"Bad credentials"}.
    if (statusCode >= 400) {
        const QString bodyText = QString::fromUtf8(responseBody);
        throw GitHubGraphqlRequestError(
            statusCode,
            networkError,
            bodyText,
            QString("HTTP error %1: %2")
                .arg(statusCode)
                .arg(bodyText)
            );
    }

    // Handle Qt-level errors that did not come with an HTTP response, such as
    // DNS failure, timeout, connection failure, or SSL errors.
    if (networkError != QNetworkReply::NoError) {
        throw GitHubGraphqlRequestError(
            0,
            networkError,
            QString(),
            QString("Network error: %1").arg(errorString)
            );
    }

    // Parse the response body as JSON.
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(responseBody, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        throw std::runtime_error(
            QString("JSON parse error: %1")
                .arg(parseError.errorString())
                .toStdString()
            );
    }

    // GitHub GraphQL responses are JSON objects.
    QJsonObject root = doc.object();

    // A GraphQL response may return HTTP 200 but still include
    // GraphQL-level errors in the "errors" field.
    if (root.contains("errors")) {
        throw std::runtime_error(
            QString("GraphQL errors: %1")
                .arg(QString::fromUtf8(
                    QJsonDocument(root["errors"].toArray()).toJson()
                    ))
                .toStdString()
            );
    }

    // Expected successful GraphQL response shape:
    //
    // {
    //   "data": {
    //      ...
    //   }
    // }
    if (!root.contains("data") || !root["data"].isObject()) {
        throw std::runtime_error(
            "GraphQL response did not contain data"
            );
    }

    // Return only the inner "data" object
    return root["data"].toObject();
}

QJsonObject runQueryWithTokenRotation(QNetworkAccessManager &manager,
    const QStringList &tokenList,
    int &tokenIndex,
    const QString &query,
    const QJsonObject &variables,
    QJsonArray *attemptLog){

    if (tokenList.isEmpty()) {
        throw std::runtime_error("No GitHub tokens are configured");
    }

    const int tokenCount = tokenList.size();
    int checkedTokens = 0;
    int usableTokens = 0;

    while (checkedTokens < tokenCount) {
        tokenIndex = tokenIndex % tokenCount;
        const int currentIndex = tokenIndex;
        const QString token = normalizeGitHubToken(tokenList.at(currentIndex));
        ++checkedTokens;

        if (token.isEmpty() || isRejectedToken(token)) {
            rotateToken(tokenIndex, tokenList);
            continue;
        }

        ++usableTokens;
        QJsonArray tokenAttempts;

        try {
            const QJsonObject data = runQuery(manager,
                                              token,
                                              query,
                                              variables,
                                              attemptLog ? &tokenAttempts : nullptr);

            if (attemptLog != nullptr) {
                for (const QJsonValue &value : tokenAttempts) {
                    QJsonObject entry = value.toObject();
                    entry["attempt"] = attemptLog->size() + 1;
                    entry["token_index"] = currentIndex + 1;
                    attemptLog->append(entry);
                }
            }

            rotateToken(tokenIndex, tokenList);
            return data;
        } catch (const GitHubGraphqlRequestError &error) {
            if (attemptLog != nullptr) {
                for (const QJsonValue &value : tokenAttempts) {
                    QJsonObject entry = value.toObject();
                    entry["attempt"] = attemptLog->size() + 1;
                    entry["token_index"] = currentIndex + 1;
                    attemptLog->append(entry);
                }
            }

            if (isAuthenticationFailure(error)) {
                // Do not keep cycling an expired/revoked token through every
                // later GraphQL page in this process. No extra validation API
                // request is made: a token is quarantined only after GitHub has
                // already rejected a real request.
                rejectToken(token);
                qWarning() << "GitHub rejected token" << (currentIndex + 1)
                           << "during GraphQL authentication; trying the next"
                              " configured token.";
                rotateToken(tokenIndex, tokenList);
                continue;
            }

            rotateToken(tokenIndex, tokenList);
            throw;
        }
    }

    if (usableTokens == 0) {
        throw std::runtime_error(
            "No usable GitHub tokens remain. Check for empty, expired, "
            "revoked, or previously rejected tokens."
            );
    }

    throw std::runtime_error(
        "GitHub authentication failed for every configured token."
        );
}

windowSliceResult crawlSearchSlice(QNetworkAccessManager &manager,
                            const QString &repo,
                            const QDateTime &sliceStart,
                            const QDateTime &sliceEnd,
                            const QStringList &tokenList,
                            int& ct,
                            const QStringList &bugKeywords,
                            const QDateTime &globalStart,
                            const QDateTime &globalEnd,
                            bool printRows = true){
    const QString startDateStr = dateString(sliceStart);
    const QString endDateStr = dateString(sliceEnd);
    const QString query = buildQuery(repo, startDateStr, endDateStr);

    windowSliceResult result;

    QString after;
    bool hasAfter = false;
    bool issueCountSet = false;

    QTextStream out(stdout);

    while (true) {
        ct = ct % tokenList.size();

        QJsonObject variables;

        if (hasAfter) {
            variables["after"] = after;
        } else {
            variables["after"] = QJsonValue::Null;
        }

        QJsonObject data = runQueryWithTokenRotation(
            manager,
            tokenList,
            ct,
            query,
            variables
            );

        QJsonObject searchBlock = data["search"].toObject();

        if (!issueCountSet) {
            result.issueCount = searchBlock["issueCount"].toInt(0);
            issueCountSet = true;
        }

        if (!printRows) {
            qInfo().noquote() << "Not outputting obtained issues - just getting issue count";
            return result;
        }

        QJsonArray nodes = searchBlock["nodes"].toArray();

        for (const QJsonValue &nodeValue : nodes) {
            if (!nodeValue.isObject()) {
                continue;
            }

            QJsonObject node = nodeValue.toObject();

            const int prNumber = node["number"].toInt();
            const QString prTitle = node["title"].toString();
            const QString prCreated = node["createdAt"].toString();
            const QJsonValue mergeChecking = node["mergedAt"];

            if (mergeChecking.isNull() || mergeChecking.isUndefined()) {
                continue;
            }

            const QString mergedValue = node["mergedAt"].toString();

            // The window is applied to the merge date, not the creation date:
            // a PR opened before the window but merged inside it belongs to it.
            QDateTime prMergeDate = parseIsoZ(mergedValue);

            if (prMergeDate < globalStart || prMergeDate > globalEnd) {
                continue;
            }

            const QString lowerTitle = prTitle.toLower();

            bool hasBugKeyword = false;
            for (const QString &keyword : bugKeywords) {
                if (lowerTitle.contains(keyword.toLower())) {
                    hasBugKeyword = true;
                    break;
                }
            }

            if (!hasBugKeyword) {
                continue;
            }

            if (prSet.contains(QString("%1").arg(prNumber))) {
                continue;
            }

            result.patches.title.append(prTitle);
            result.patches.prs.append(QString("%1").arg(prNumber));
            prSet.insert(QString("%1").arg(prNumber));

            out << prNumber << "," << prCreated << "," << prTitle << Qt::endl;
        }

        QJsonObject pageInfo = searchBlock["pageInfo"].toObject();
        const bool hasNextPage = pageInfo["hasNextPage"].toBool(false);

        if (hasNextPage) {
            after = pageInfo["endCursor"].toString();
            hasAfter = true;
        } else {
            break;
        }
    }

    return result;
}

prPatches adaptiveWindowedCrawl(QNetworkAccessManager &manager,
    const QString &repo,
    const QDateTime &startDateTime,
    const QDateTime &endDateTime,
    const QStringList &tokenList,
    int& ct,
    const QStringList &bugKeywords,
    int initialDays = 90,
    int denseThreshold = 900,
    int sparseThreshold = 100,
    int minDays = 1,
    int maxDays = 365){
    prPatches result;

    QDateTime currentStart = startDateTime;
    int windowDays = initialDays;

    while (currentStart <= endDateTime) {
        QDateTime currentEnd = currentStart.addDays(windowDays - 1);

        if (currentEnd > endDateTime) {
            currentEnd = endDateTime;
        }

        windowSliceResult probe = crawlSearchSlice(manager,
            repo,
            currentStart,
            currentEnd,
            tokenList,
            ct,
            bugKeywords,
            startDateTime,
            endDateTime,
            false);

        if (probe.issueCount >= denseThreshold && windowDays > minDays) {
            windowDays = qMax(minDays, windowDays / 2);
            continue;
        }

        if (probe.issueCount <= sparseThreshold && windowDays < maxDays) {
            windowDays = qMin(maxDays, static_cast<int>(windowDays * 1.5));
        }

        windowSliceResult slice = crawlSearchSlice(
            manager,
            repo,
            currentStart,
            currentEnd,
            tokenList,
            ct,
            bugKeywords,
            startDateTime,
            endDateTime,
            true);

        result.prs.append(slice.patches.prs);
        result.title.append(slice.patches.title);

        currentStart = currentEnd.addDays(1);
    }

    prSet.clear();
    return result;
}

prPatches getPrPatches(const QString &repository, const QString &divergenceDate,
                       const QString &leastDate, const QStringList &tokenList, int &ct,
                       const PrScanProgress &onPage) {
    const QDateTime start = parseIsoZ(divergenceDate);
    const QDateTime end = parseIsoZ(leastDate);
    if (start > end) throw std::runtime_error("PR start date is after cutoff date");
    if (tokenList.isEmpty()) throw std::runtime_error("No GitHub tokens configured");
    const QStringList ownerAndName = repository.trimmed().split('/', Qt::SkipEmptyParts);
    if (ownerAndName.size() != 2)
        throw std::runtime_error(QString("Expected an owner/name repository, got: %1")
                                     .arg(repository).toStdString());

    QElapsedTimer timer;
    timer.start();
    prPatches result;
    QSet<QString> seen;

    // Enumerate the repository's merged PRs directly instead of using search,
    // which can omit merged PRs. Eligibility is decided by mergedAt, so a PR
    // created long before the window but merged inside it is still found.
    //
    // Compared with REST /pulls this is much cheaper per page: only merged PRs
    // are returned (no closed-unmerged ones) and only four fields per PR
    // instead of the full PR object (~17 KB each).
    //
    // Pages run oldest-created first. A PR merged inside the window must have
    // been created no later than the cutoff, so the walk stops at the first PR
    // created after it. A PR merged while this runs has mergedAt after the
    // cutoff and cannot change the result.
    static const QString query = QStringLiteral(R"GRAPHQL(
        query($owner: String!, $name: String!, $after: String) {
          repository(owner: $owner, name: $name) {
            pullRequests(states: MERGED, first: 100, after: $after,
                         orderBy: {field: CREATED_AT, direction: ASC}) {
              totalCount
              nodes { number title createdAt mergedAt }
              pageInfo { hasNextPage endCursor }
            }
          }
        }
        )GRAPHQL");

    QNetworkAccessManager manager;
    QJsonObject variables;
    variables["owner"] = ownerAndName[0];
    variables["name"] = ownerAndName[1];
    variables["after"] = QJsonValue::Null;

    int totalMerged = -1;
    int scanned = 0;
    int inWindow = 0;
    bool reachedCutoff = false;

    for (int page = 1; !reachedCutoff; ++page) {
        const QJsonObject data =
            runQueryWithTokenRotation(manager, tokenList, ct, query, variables);

        const QJsonValue repoValue = data.value("repository");
        if (!repoValue.isObject())
            throw std::runtime_error(QString("GitHub repository not found: %1")
                                         .arg(repository).toStdString());
        const QJsonObject connection =
            repoValue.toObject().value("pullRequests").toObject();
        if (totalMerged < 0) totalMerged = connection.value("totalCount").toInt();

        QString lastCreated;
        const QJsonArray nodes = connection.value("nodes").toArray();
        for (const QJsonValue &value : nodes) {
            const QJsonObject pr = value.toObject();
            if (!pr.value("createdAt").isString() || !pr.value("mergedAt").isString())
                throw std::runtime_error("GitHub PR is missing createdAt/mergedAt metadata");
            if (!pr.value("title").isString() || !pr.value("number").isDouble())
                throw std::runtime_error("GitHub PR is missing title/number metadata");

            lastCreated = pr.value("createdAt").toString();
            if (parseIsoZ(lastCreated) > end) { reachedCutoff = true; break; }
            ++scanned;

            const QDateTime merged = parseIsoZ(pr.value("mergedAt").toString());
            if (merged < start || merged > end) continue;
            ++inWindow;

            const QString title = pr.value("title").toString();
            bool matches = false;
            for (const QString &keyword : constant::BUGFIX_KEYWORDS) {
                if (title.toLower().contains(keyword.toLower())) { matches = true; break; }
            }
            const QString number = QString::number(pr.value("number").toInt());
            if (!matches || seen.contains(number)) continue;
            seen.insert(number);
            result.prs.append(number);
            result.title.append(title);
        }

        qInfo().noquote()
            << QString("PR scan %1: page %2, %3/%4 merged PRs read, created up to %5, "
                       "%6 merged in window, %7 bug-fix matches (%8 s)")
                   .arg(repository).arg(page).arg(scanned).arg(totalMerged)
                   .arg(lastCreated.isEmpty() ? QStringLiteral("-") : lastCreated)
                   .arg(inWindow).arg(result.prs.size())
                   .arg(timer.elapsed() / 1000.0, 0, 'f', 1);

        if (onPage) {
            const int maxPages = qMax(1, (totalMerged + 99) / 100);
            onPage(page, qMax(maxPages, page), result.prs.size());
        }

        const QJsonObject pageInfo = connection.value("pageInfo").toObject();
        if (!pageInfo.value("hasNextPage").toBool()) break;
        variables["after"] = pageInfo.value("endCursor").toString();
    }

    result.seconds = timer.elapsed() / 1000.0;
    qInfo().noquote()
        << QString("PR scan %1 done: %2 merged in window, %3 bug-fix PRs, %4 s")
               .arg(repository).arg(inWindow).arg(result.prs.size())
               .arg(result.seconds, 0, 'f', 1);
    return result;
}
