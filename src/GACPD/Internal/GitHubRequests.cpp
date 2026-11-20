#include "headers/GACPD/Internal/GitHubRequests.h"

#include "headers/GACPD/Methods/common.h"
#include "headers/GACPD/Methods/helpers_GACPD.h"
#include "headers/GACPD/Methods/patchExtractor.h"
#include <QDebug>
#include <QNetworkAccessManager>
#include <QThread>
#include <stdexcept>

static QJsonValue responseOrNull(const QJsonValue &response){
    return response.isUndefined() ? QJsonValue(QJsonValue::Null) : response;
}

QJsonValue lastAttemptResponse(const QJsonArray &attempts){
    if (attempts.isEmpty() || !attempts.last().isObject()) {
        return QJsonValue(QJsonValue::Null);
    }

    return attempts.last().toObject().value("response");
}

QJsonObject makeRequestLog(const QString &url,
                           const QJsonArray &attempts,
                           const QJsonValue &successfulResponse){
    QJsonObject log;
    log["url"] = url;
    log["attempts"] = attempts;

    if (!successfulResponse.isUndefined()) {
        log["response"] = responseOrNull(successfulResponse);
    } else {
        log["response"] = lastAttemptResponse(attempts);
    }

    return log;
}

std::optional<QJsonObject> fetchGitHubObjectWithRotation(
    const QString &url, const QStringList &tokenList, int &ct, int &requestCount,
    const QStringList &requiredKeys, QJsonArray *attemptLog) {
    for (int attempt = 0; attempt < 3; ++attempt) {
        try {
            const QJsonValue response = githubRestRequest(url, tokenList, ct, false, attemptLog, &requestCount);
            if (response.isObject()) {
                const QJsonObject object = response.toObject();
                bool valid = !object.contains("message");
                for (const QString &key : requiredKeys) {
                    if (!object.contains(key) || object.value(key).isNull()) valid = false;
                    if (key == "changed_files" &&
                        (!object.value(key).isDouble() || object.value(key).toDouble() < 0)) valid = false;
                }
                if (valid) return object;
            }
        } catch (const std::exception &error) { qWarning() << error.what(); }
        if (attempt < 2) QThread::msleep(200 * (attempt + 1));
    }
    return std::nullopt;
}

std::optional<QJsonArray> fetchGitHubArrayWithRotation(
    const QString &url, const QStringList &tokenList, int &ct, int &requestCount,
    QJsonArray *attemptLog) {
    try {
        const QJsonValue response = githubRestRequest(url, tokenList, ct, false, attemptLog, &requestCount);
        if (response.isArray()) return response.toArray();
    } catch (const std::exception &error) { qWarning() << error.what(); }
    return std::nullopt;
}

GraphqlCommitFetchResult fetchAllPullRequestCommitsGraphql(
    const QString &repo,
    const QString &pullrequest,
    const QStringList &tokenList,
    int &ct,
    int &requestCount,
    bool captureRequestLogs){
    GraphqlCommitFetchResult result;

    const QStringList repoParts = repo.split('/', Qt::SkipEmptyParts);
    if (repoParts.size() != 2 || tokenList.isEmpty()) {
        return result;
    }

    bool prNumberOk = false;
    const int prNumber = pullrequest.toInt(&prNumberOk);
    if (!prNumberOk) {
        return result;
    }

    const QString query = QStringLiteral(R"GRAPHQL(
        query($owner: String!, $name: String!, $number: Int!, $after: String) {
          repository(owner: $owner, name: $name) {
            pullRequest(number: $number) {
              commits(first: 100, after: $after) {
                totalCount
                nodes {
                  commit {
                    oid
                    abbreviatedOid
                    message
                    messageHeadline
                    messageBody
                    authoredDate
                    committedDate
                    additions
                    deletions
                    url
                  }
                }
                pageInfo {
                  hasNextPage
                  endCursor
                }
              }
            }
          }
        }
    )GRAPHQL");

    QNetworkAccessManager manager;
    QString after;
    bool hasAfter = false;

    while (true) {
        QJsonObject variables;
        variables["owner"] = repoParts.at(0);
        variables["name"] = repoParts.at(1);
        variables["number"] = prNumber;
        variables["after"] = hasAfter
            ? QJsonValue(after)
            : QJsonValue(QJsonValue::Null);

        QJsonObject requestLog;
        requestLog["url"] = "https://api.github.com/graphql";
        requestLog["query"] = query;
        requestLog["variables"] = variables;

        ct = ct % tokenList.size();
        QJsonArray graphqlAttempts;

        try {
            const QJsonObject data = runQueryWithTokenRotation(
                manager,
                tokenList,
                ct,
                query,
                variables,
                &graphqlAttempts
                );
            requestCount += qMax(1, int(graphqlAttempts.size()));
            if (captureRequestLogs) {
                requestLog["attempts"] = graphqlAttempts;
                requestLog["response"] = data;
                result.requestLogs.append(requestLog);
            }

            const QJsonObject repository = data.value("repository").toObject();
            const QJsonObject pr = repository.value("pullRequest").toObject();
            const QJsonObject commitsBlock = pr.value("commits").toObject();

            if (commitsBlock.isEmpty()) {
                return result;
            }

            result.totalCount = commitsBlock.value("totalCount").toInt();

            const QJsonArray nodes = commitsBlock.value("nodes").toArray();
            for (const QJsonValue &nodeValue : nodes) {
                const QJsonObject node = nodeValue.toObject();
                const QJsonObject commit = node.value("commit").toObject();
                if (!commit.isEmpty()) {
                    result.commits.append(commit);
                }
            }

            const QJsonObject pageInfo =
                commitsBlock.value("pageInfo").toObject();
            const bool hasNextPage =
                pageInfo.value("hasNextPage").toBool(false);

            if (!hasNextPage) {
                result.complete =
                    result.commits.size() == result.totalCount;
                return result;
            }

            after = pageInfo.value("endCursor").toString();
            if (after.isEmpty()) {
                return result;
            }

            hasAfter = true;
        } catch (const std::exception &e) {
            requestCount += qMax(1, int(graphqlAttempts.size()));
            if (captureRequestLogs) {
                requestLog["attempts"] = graphqlAttempts;
                requestLog["error"] = QString::fromUtf8(e.what());
                result.requestLogs.append(requestLog);
            }
            return result;
        }
    }
}
