#ifndef GITHUB_REQUESTS_H
#define GITHUB_REQUESTS_H

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>
#include <optional>

// Talking to GitHub with a pool of tokens.
//
// Every request is retried once per token before giving up, so a run survives
// a token going over its rate limit. Each attempt is recorded so the captured
// API responses show what was asked and what came back, including the failures.
//
// GitHub's REST endpoint for pull request commits stops at 250 entries, so
// larger pull requests fall back to paging the GraphQL commit connection.

// The response body of the final attempt in a request log.

// The response body of the final attempt in a request log.
QJsonValue lastAttemptResponse(const QJsonArray &attempts);

// One record for the captured API response file: what was requested, every
// attempt it took, and the response that was finally used.
QJsonObject makeRequestLog(const QString &url,
                           const QJsonArray &attempts,
                           const QJsonValue &successfulResponse =
                               QJsonValue(QJsonValue::Undefined));

// Fetches a JSON object, rotating through the tokens until one of them
// returns every key in `requiredKeys`. `ct` is the token index to start
// from and is left on the token that worked.
std::optional<QJsonObject> fetchGitHubObjectWithRotation(
    const QString &url,
    const QStringList &tokenList,
    int &ct,
    int &requestCount,
    const QStringList &requiredKeys,
    QJsonArray *attemptLog = nullptr);

// The same, for endpoints that return an array.
std::optional<QJsonArray> fetchGitHubArrayWithRotation(
    const QString &url,
    const QStringList &tokenList,
    int &ct,
    int &requestCount,
    QJsonArray *attemptLog = nullptr);

// Outcome of paging a pull request's commits through GraphQL. `complete` is
// false when paging stopped early, in which case `commits` is partial.
struct GraphqlCommitFetchResult {
    QJsonArray commits;
    QJsonArray requestLogs;
    int totalCount = 0;
    bool complete = false;
};

// Every commit on a pull request. Used when the REST endpoint hits its
// 250-commit ceiling and cannot report the rest.
GraphqlCommitFetchResult fetchAllPullRequestCommitsGraphql(
    const QString &repo,
    const QString &pullrequest,
    const QStringList &tokenList,
    int &ct,
    int &requestCount,
    bool captureRequestLogs);

#endif // GITHUB_REQUESTS_H
