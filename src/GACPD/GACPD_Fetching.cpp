// GACPD stage 1 and 2: finding the pull requests in range and fetching
// everything GitHub knows about them.
//
// Every response used is also kept, so a run can be audited afterwards without
// calling GitHub again. See the "GitHub API response capture" section of the
// README for the shape of that file.

#include "headers/GACPD/GACPD.h"

#include "headers/GACPD/Internal/FetchedPrData.h"
#include "headers/GACPD/Internal/FileLanguages.h"
#include "headers/GACPD/Internal/GitHubRequests.h"
#include "headers/GACPD/Internal/JscpdEvidence.h"
#include "headers/GACPD/Internal/OrderedJson.h"
#include "headers/GACPD/Internal/ResultRecords.h"
#include "headers/GACPD/Methods/helpers_GACPD.h"
#include "headers/GACPD/Methods/common.h"
#include "headers/GACPD/Methods/patchExtractor.h"
#include "headers/GACPD/Methods/appPath.h"

#include <QCryptographicHash>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QTemporaryDir>
#include <QThread>
#include <exception>
#include <stdexcept>

prDataExtractor GACPD::fetchPullRequestData(const QString &mainline,
                                                   const QString &variant,
                                                   const QStringList &pullrequests,
                                                   const QString &variantSha,
                                                   const QStringList &tokenList,
                                                   int& ct){
    Q_UNUSED(variant);

    #ifdef QT_DEBUG
        qDebug() << "Fetching files and commit information from patches...";
    #endif

    QElapsedTimer timer;
    timer.start();

    int req = 0;
    QMap<QString, QVariantMap> pullrequestData;
    quint32 sizePr = pullrequests.size();
    quint32 i = 1;

    const QString auditDir = movisResultsPath(rootFolder);
    QDir().mkpath(auditDir);

    const QString apiResponseLogPath =
        auditDir + "/github_api_responses.json";
    QJsonObject allPrApiResponses;
    // Keep the complete API-response log in memory while PRs are fetched.
    // Serializing the ever-growing object after every PR causes repeated,
    // increasingly expensive full-file rewrites. Flush it once after the run.

    QFile auditFile(auditDir + "/github_fetch_audit.csv");
    const bool auditOpen =
        auditFile.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream auditOut(&auditFile);

    if (auditOpen) {
        auditOut.setEncoding(QStringConverter::Utf8);
        auditOut
            << "pr,pr_api_ok,github_changed_files,fetched_files,"
               "files_fetch_complete,included,status\n";
    }

    qint64 totalGithubChangedFiles = 0;
    qint64 totalFetchedFiles = 0;
    int includedPrs = 0;
    int failedPrMetadata = 0;
    int incompleteFileFetches = 0;

    for (const QString &pullrequest : pullrequests) {
        emit stageChanged(QStringLiteral("[%1/%2] Fetching pull request data...")
                              .arg(i)
                              .arg(sizePr));
        ++i;

        const QString prRequest =
            QString("%1%2/pulls/%3")
                .arg(constant::GITHUB_API_BASE_URL, mainline, pullrequest);

        QJsonObject prApiLog;
        QJsonArray prRequestAttempts;
        QJsonArray *prAttemptsLog =
            generateApiJsonFile ? &prRequestAttempts : nullptr;

        const std::optional<QJsonObject> maybePr = fetchGitHubObjectWithRotation(
            prRequest,
            tokenList,
            ct,
            req,
            {"number", "changed_files", "base", "head"},
            prAttemptsLog
            );

        if (generateApiJsonFile) {
            prApiLog["request"] = makeRequestLog(
                prRequest,
                prRequestAttempts,
                maybePr.has_value()
                    ? QJsonValue(maybePr.value())
                    : QJsonValue(QJsonValue::Undefined)
                );
            prApiLog["response"] = maybePr.has_value()
                ? QJsonValue(maybePr.value())
                : lastAttemptResponse(prRequestAttempts);
            prApiLog["commit_list_requested"] = fetchPrCommitList;
        }

        if (!maybePr.has_value()) {
            ++failedPrMetadata;

            if (generateApiJsonFile) {
                prApiLog["status"] = "PR_METADATA_FETCH_FAILED";
                allPrApiResponses[pullrequest] = prApiLog;
            }
            if (auditOpen) {
                auditOut << pullrequest
                         << ",0,0,0,0,0,PR_METADATA_FETCH_FAILED\n";
            }

            continue;
        }

        const QJsonObject pr = maybePr.value();
        int githubChangedFiles = pr.value("changed_files").toInt();
        totalGithubChangedFiles += githubChangedFiles;

        QVariantMap prData;
        QVariantList commitsDataList;

        prData["pr_url"] = prRequest;
        prData["created_at"] = pr.value("created_at").toString();
        prData["merged_at"] = pr.value("merged_at").toString();
        prData["base_sha_added"] =
            pr.value("base").toObject().value("sha").toString();
        prData["head_sha_added"] =
            pr.value("head").toObject().value("sha").toString();
        prData["merge_commit_sha"] =
            pr.value("merge_commit_sha").toString();
        prData["commits"] = pr.value("commits").toInt();
        prData["changed_files"] = githubChangedFiles;
        prData["destination_sha"] = variantSha;
        prData["html_url"] = pr.value("html_url").toString();
        prData["title"] = pr.value("title").toString();
        prData["body"] = pr.value("body").toString();

        // Get the commit SHA immediately before PR creation. This metadata is
        // part of the normal classification path and is separate from the
        // optional full PR commit-list fetch.
        const QString prCreatedAt = pr.value("created_at").toString();
        const QString commitBeforeUrl =
            QString("%1%2/commits?page=1&per_page=1&until=%3")
                .arg(constant::GITHUB_API_BASE_URL, mainline, prCreatedAt);

        QJsonArray commitBeforeAttempts;
        QJsonArray *commitBeforeAttemptsLog =
            generateApiJsonFile ? &commitBeforeAttempts : nullptr;

        const std::optional<QJsonArray> maybeLastCommitShaArray = fetchGitHubArrayWithRotation(
            commitBeforeUrl,
            tokenList,
            ct,
            req,
            commitBeforeAttemptsLog
            );

        if (generateApiJsonFile) {
            prApiLog["commit_before"] = makeRequestLog(
                commitBeforeUrl,
                commitBeforeAttempts,
                maybeLastCommitShaArray.has_value()
                    ? QJsonValue(maybeLastCommitShaArray.value())
                    : QJsonValue(QJsonValue::Undefined)
                );
        }

        if (maybeLastCommitShaArray.has_value() &&
            !maybeLastCommitShaArray.value().isEmpty()) {
            prData["commit_sha_before"] =
                maybeLastCommitShaArray.value()
                    .first().toObject().value("sha").toString();
        } else {
            prData["commit_sha_before"] = QString();
        }

        if (generateApiJsonFile) {
            prApiLog["commit_sha_before"] =
                prData.value("commit_sha_before").toString();
        }

        // Full PR commit lists are optional because they are not required by
        // the classifier. For PRs at or below GitHub's 250-commit REST cap,
        // page through REST. Above 250, go directly to cursor-paginated
        // GraphQL so we do not waste REST calls on data GraphQL must refetch.
        if (fetchPrCommitList) {
            const int githubPrCommitCount = pr.value("commits").toInt();
            bool commitsFetchComplete = true;
            int fetchedPrCommitCount = 0;
            QJsonObject commitResponses;
            QJsonArray commitRequestLogs;
            QJsonArray commitGraphqlRequestLogs;

            if (githubPrCommitCount > 250) {
                const GraphqlCommitFetchResult graphqlCommits =
                    fetchAllPullRequestCommitsGraphql(
                        mainline,
                        pullrequest,
                        tokenList,
                        ct,
                        req,
                        generateApiJsonFile
                        );

                if (generateApiJsonFile) {
                    commitGraphqlRequestLogs = graphqlCommits.requestLogs;
                }

                QSet<QString> uniqueCommitIds;
                int unnamedCommitIndex = 0;
                for (const QJsonValue &commitValue : graphqlCommits.commits) {
                    const QJsonObject commitObject = commitValue.toObject();
                    QString commitKey = commitObject.value("oid").toString();

                    if (commitKey.isEmpty()) {
                        commitKey = QString("entry_%1").arg(++unnamedCommitIndex);
                    }

                    uniqueCommitIds.insert(commitKey);

                    if (generateApiJsonFile) {
                        QJsonObject commitLog;
                        commitLog["response"] = commitValue;
                        commitLog["source"] = "graphql_pull_request_commits";
                        commitResponses[commitKey] = commitLog;
                    }
                }

                fetchedPrCommitCount = uniqueCommitIds.size();
                commitsFetchComplete =
                    graphqlCommits.complete &&
                    graphqlCommits.totalCount == githubPrCommitCount &&
                    fetchedPrCommitCount == githubPrCommitCount;
            } else {
                QJsonArray pullrequestCommits;
                constexpr int commitsPerPage = 100;
                constexpr int maxPrCommitPages = 3;

                for (int page = 1;
                     page <= maxPrCommitPages &&
                     pullrequestCommits.size() < githubPrCommitCount;
                     ++page) {
                    const QString commitsUrl =
                        QString("%1%2/pulls/%3/commits?page=%4&per_page=%5")
                            .arg(constant::GITHUB_API_BASE_URL,
                                 mainline,
                                 pullrequest)
                            .arg(page)
                            .arg(commitsPerPage);

                    QJsonArray commitPageAttempts;
                    QJsonArray *commitPageAttemptsLog =
                        generateApiJsonFile ? &commitPageAttempts : nullptr;
                    const std::optional<QJsonArray> maybeCommitPage = fetchGitHubArrayWithRotation(
                        commitsUrl,
                        tokenList,
                        ct,
                        req,
                        commitPageAttemptsLog
                        );

                    if (generateApiJsonFile) {
                        QJsonObject requestLog = makeRequestLog(
                            commitsUrl,
                            commitPageAttempts,
                            maybeCommitPage.has_value()
                                ? QJsonValue(maybeCommitPage.value())
                                : QJsonValue(QJsonValue::Undefined)
                            );
                        requestLog["page"] = page;
                        commitRequestLogs.append(requestLog);
                    }

                    if (!maybeCommitPage.has_value()) {
                        commitsFetchComplete = false;
                        break;
                    }

                    const QJsonArray commitPage = maybeCommitPage.value();
                    for (const QJsonValue &commitValue : commitPage) {
                        pullrequestCommits.append(commitValue);
                    }

                    if (commitPage.size() < commitsPerPage) {
                        break;
                    }
                }

                QSet<QString> uniqueCommitIds;
                int unnamedCommitIndex = 0;
                for (const QJsonValue &commitValue : pullrequestCommits) {
                    const QJsonObject commitObject = commitValue.toObject();
                    QString commitKey = commitObject.value("sha").toString();

                    if (commitKey.isEmpty()) {
                        commitKey = QString("entry_%1").arg(++unnamedCommitIndex);
                    }

                    uniqueCommitIds.insert(commitKey);

                    if (generateApiJsonFile) {
                        QJsonObject commitLog;
                        commitLog["response"] = commitValue;
                        commitLog["source"] = "rest_pull_request_commits";
                        commitResponses[commitKey] = commitLog;
                    }
                }

                fetchedPrCommitCount = uniqueCommitIds.size();
                if (fetchedPrCommitCount != githubPrCommitCount) {
                    commitsFetchComplete = false;
                }
            }

            if (generateApiJsonFile) {
                prApiLog["commits"] = commitResponses;
                prApiLog["commit_requests"] = commitRequestLogs;
                prApiLog["commit_graphql_requests"] = commitGraphqlRequestLogs;
                prApiLog["github_pr_commit_count"] = githubPrCommitCount;
                prApiLog["fetched_pr_commits"] = fetchedPrCommitCount;
                prApiLog["commits_fetch_complete"] = commitsFetchComplete;

                if (githubPrCommitCount > 250) {
                    prApiLog["commits_fetch_note"] = commitsFetchComplete
                        ? "PR exceeds the REST 250-commit cap; complete commit list captured with cursor-paginated GraphQL"
                        : "PR exceeds the REST 250-commit cap and GraphQL did not recover the complete commit set";
                } else if (!commitsFetchComplete) {
                    prApiLog["commits_fetch_note"] =
                        "Commit response fetch failed or returned fewer commits than PR metadata reported";
                }
            }
        } else if (generateApiJsonFile) {
            prApiLog["github_pr_commit_count"] = pr.value("commits").toInt();
            prApiLog["commits_fetch_complete"] = QJsonValue(QJsonValue::Null);
            prApiLog["commits_fetch_note"] =
                "Full PR commit-list fetch disabled for this run";
        }

        // GitHub returns PR files in pages of at most 100 and caps this
        // endpoint at 3000 files. Never accept a failed page as an empty page;
        // that would silently produce PRs with 0/partial file lists after a
        // rate-limit/network/server error.
        QJsonArray pullrequestFilesMerged;
        QJsonArray fileRequestLogs;
        constexpr int filesPerPage = 100;
        constexpr int maxFilePages = 30;
        bool filesFetchComplete = true;

        // A file list that disagrees with changed_files may be a stale or
        // inconsistent snapshot; refresh the PR metadata and refetch every
        // page instead of accepting a partial list.
        for (int consistencyAttempt = 0; consistencyAttempt < 3; ++consistencyAttempt) {
            if (consistencyAttempt > 0) {
                QThread::msleep(200 * consistencyAttempt);
                const std::optional<QJsonObject> refreshed = fetchGitHubObjectWithRotation(
                    prRequest, tokenList, ct, req, {"number", "changed_files", "base", "head"}, prAttemptsLog);
                if (!refreshed.has_value()) { filesFetchComplete = false; continue; }
                totalGithubChangedFiles -= githubChangedFiles;
                githubChangedFiles = refreshed->value("changed_files").toInt();
                totalGithubChangedFiles += githubChangedFiles;
                prData["changed_files"] = githubChangedFiles;
            }
            pullrequestFilesMerged = QJsonArray();
            filesFetchComplete = true;
            for (int page = 1; page <= maxFilePages; ++page) {
                const QString filesMergedUrl =
                    QString("%1%2/pulls/%3/files?page=%4&per_page=%5")
                        .arg(constant::GITHUB_API_BASE_URL, mainline, pullrequest)
                        .arg(page)
                        .arg(filesPerPage);

                QJsonArray filePageAttempts;
                QJsonArray *filePageAttemptsLog =
                    generateApiJsonFile ? &filePageAttempts : nullptr;
                const std::optional<QJsonArray> maybePageFiles = fetchGitHubArrayWithRotation(
                    filesMergedUrl,
                    tokenList,
                    ct,
                    req,
                    filePageAttemptsLog
                    );

                if (generateApiJsonFile) {
                    QJsonObject requestLog = makeRequestLog(
                        filesMergedUrl,
                        filePageAttempts,
                        maybePageFiles.has_value()
                            ? QJsonValue(maybePageFiles.value())
                            : QJsonValue(QJsonValue::Undefined)
                        );
                    requestLog["page"] = page;
                    fileRequestLogs.append(requestLog);
                }

                if (!maybePageFiles.has_value()) {
                    filesFetchComplete = false;
                    break;
                }

                const QJsonArray pageFiles = maybePageFiles.value();

                for (const QJsonValue &fileValue : pageFiles) {
                    pullrequestFilesMerged.append(fileValue);
                }

                if (pageFiles.size() < filesPerPage) {
                    break;
                }
            }

            if (filesFetchComplete && pullrequestFilesMerged.size() == githubChangedFiles) break;
        }

        const int fetchedFiles = pullrequestFilesMerged.size();
        totalFetchedFiles += fetchedFiles;

        if (generateApiJsonFile) {
            QJsonObject fileResponses;
            int unnamedFileIndex = 0;
            for (const QJsonValue &fileValue : pullrequestFilesMerged) {
                const QJsonObject fileObject = fileValue.toObject();
                QString fileKey = fileObject.value("filename").toString();

                if (fileKey.isEmpty()) {
                    fileKey = QString("entry_%1").arg(++unnamedFileIndex);
                }

                QJsonObject fileLog;
                fileLog["response"] = fileValue;
                fileResponses[fileKey] = fileLog;
            }

            prApiLog["files"] = fileResponses;
            prApiLog["file_requests"] = fileRequestLogs;
            prApiLog["github_changed_files"] = githubChangedFiles;
            prApiLog["fetched_files"] = fetchedFiles;
            prApiLog["files_fetch_complete"] =
                filesFetchComplete && githubChangedFiles == fetchedFiles;
        }

        // changed_files is the authoritative expected count for the PR. If
        // it differs, do not classify a partial PR and pretend the dataset is
        // complete. Record it in the audit CSV and omit it from repoData.
        if (!filesFetchComplete || githubChangedFiles != fetchedFiles) {
            ++incompleteFileFetches;

            if (generateApiJsonFile) {
                prApiLog["status"] = "INCOMPLETE_FILE_FETCH";
                allPrApiResponses[pullrequest] = prApiLog;
            }
            qCritical() << "Incomplete PR file fetch for PR" << pullrequest
                        << "GitHub changed_files:" << githubChangedFiles
                        << "fetched files:" << fetchedFiles
                        << "page fetch complete:" << filesFetchComplete;

            if (auditOpen) {
                auditOut << pullrequest << ",1,"
                         << githubChangedFiles << ','
                         << fetchedFiles << ','
                         << (filesFetchComplete ? 1 : 0)
                         << ",0,INCOMPLETE_FILE_FETCH\n";
            }

            continue;
        }

        QVariantMap commitsData;

        for (const QJsonValue &fileValue : pullrequestFilesMerged) {
            const QJsonObject file = fileValue.toObject();
            const QString fileName = file.value("filename").toString();

            if (fileName.isEmpty()) {
                qWarning() << "Skipping PR file entry with empty filename in PR"
                           << pullrequest;
                continue;
            }

            QVariantMap sub;
            sub["status"] = file.value("status").toString();
            sub["additions"] = file.value("additions").toInt();
            sub["deletions"] = file.value("deletions").toInt();
            sub["changes"] = file.value("changes").toInt();

            // GitHub may omit the patch field for large diffs, binary files,
            // or other cases where textual patch content is unavailable.
            // Preserve that distinction explicitly instead of turning a
            // missing field into an empty patch and later assuming zero hunks.
            const int fileChanges = file.value("changes").toInt();
            const QString githubPatchText = file.value("patch").toString();
            const bool patchAvailable =
                fileChanges > 0 &&
                file.contains("patch") &&
                file.value("patch").isString() &&
                !githubPatchText.isEmpty();
            const QString patchStatus = patchAvailable
                ? QStringLiteral("available")
                : (fileChanges == 0
                    ? QStringLiteral("not_provided_no_textual_changes")
                    : QStringLiteral("missing_from_github"));

            sub["patch_available"] = patchAvailable;
            sub["patch_status"] = patchStatus;
            if (patchAvailable) {
                sub["patch"] = githubPatchText;
            }

            if (!patchAvailable && fileChanges > 0) {
                qWarning() << "GitHub did not provide patch text for changed file"
                           << fileName << "in PR" << pullrequest
                           << "changes:" << fileChanges;
            }

            // GitHub supplies previous_filename for renamed PR files. Keep it
            // in the fetched data instead of trying to reconstruct rename
            // metadata later from a separate local git command.
            sub["previous_filename"] =
                file.value("previous_filename").toString();

            QVariantList filePatchList;
            filePatchList.append(sub);
            commitsData[fileName] = filePatchList;
        }

        // The map must still contain every fetched GitHub filename. If this
        // ever differs, it indicates duplicate/empty filenames or an internal
        // map issue and the PR should not be silently accepted.
        if (commitsData.size() != fetchedFiles) {
            ++incompleteFileFetches;

            if (generateApiJsonFile) {
                prApiLog["status"] = "FILE_MAP_SIZE_MISMATCH";
                allPrApiResponses[pullrequest] = prApiLog;
            }
            qCritical() << "PR file-map size mismatch for PR" << pullrequest
                        << "fetched files:" << fetchedFiles
                        << "unique mapped files:" << commitsData.size();

            if (auditOpen) {
                auditOut << pullrequest << ",1,"
                         << githubChangedFiles << ','
                         << fetchedFiles
                         << ",1,0,FILE_MAP_SIZE_MISMATCH\n";
            }

            continue;
        }

        commitsDataList.append(commitsData);
        prData["commits_data"] = commitsDataList;
        pullrequestData[pullrequest] = prData;
        ++includedPrs;

        if (generateApiJsonFile) {
            prApiLog["status"] = "OK";
            allPrApiResponses[pullrequest] = prApiLog;
        }
        if (auditOpen) {
            auditOut << pullrequest << ",1,"
                     << githubChangedFiles << ','
                     << fetchedFiles
                     << ",1,1,OK\n";
        }
    }

    if (auditOpen) {
        auditFile.close();
    }

    // The raw API JSON is intentionally opt-in because retaining every raw
    // response can consume substantial memory on large datasets. When enabled,
    // write it once at the end using QSaveFile for an atomic replacement.
    if (generateApiJsonFile &&
        !writePrApiResponseLog(apiResponseLogPath, allPrApiResponses)) {
        qCritical() << "Failed to write final GitHub API response log:"
                    << apiResponseLogPath;
    }

    qInfo() << "GitHub fetch audit summary - requested PRs:" << pullrequests.size()
            << "included PRs:" << includedPrs
            << "metadata failures:" << failedPrMetadata
            << "incomplete file fetches:" << incompleteFileFetches
            << "GitHub changed_files total (valid PR metadata):"
            << totalGithubChangedFiles
            << "fetched file entries total:" << totalFetchedFiles;

    const double runtime = timer.elapsed() / 1000.0;

    prDataExtractor result;
    result.pullRequestData = pullrequestData;
    result.requestCount = req;
    result.runtimeSeconds = runtime;

    return result;
}

VariantShaExtractor GACPD::getVariantSha(const QString &variant,
                                  const QString &cutOffDate,
                                  const QStringList &tokenList,
                                  int& ct){
    VariantShaExtractor result;

    try {
        QString url = QString("%1%2/commits?page=1&per_page=1&until=%3")
        .arg(constant::GITHUB_API_BASE_URL, variant, cutOffDate);

        QJsonArray cutOffCommits = githubRestRequest(url, tokenList, ct).toArray();

        if (!cutOffCommits.isEmpty()) {
            result.variantSha = cutOffCommits.first().toObject().value("sha").toString();
        }

    } catch (const std::exception &e) {
        #ifdef QT_DEBUG
            qDebug() << "ERROR in func: getVariantSha:" << e.what();
        #endif
    } catch (...) {
        #ifdef QT_DEBUG
            qDebug() << "ERROR in func: getVariantSha: unknown error";
        #endif
    }

    return result;
}

QString GACPD::getDates(const QString& sourceRepo,
              const QString& variantRepo,
              const QStringList& tokenList,
              int &ct,
              const QString& cutOffDate,
              const QString& divergenceDate){
    RepoDateInfo results = getDivergenceDate(sourceRepo, variantRepo, tokenList,
                                          ct, cutOffDate, divergenceDate);

    // Detached pairs (different fork networks) have no merge base. Fall back
    // to the metadata estimate, keeping any date the caller supplied.
    if (results.divergenceDate.trimmed().isEmpty()) {
        qWarning().noquote()
            << QString("%1 -> %2: no merge base; treating it as a detached pair "
                       "and estimating dates from repository metadata.")
                   .arg(sourceRepo, variantRepo);
        const RepoDateInfo detached =
            getDetachedPairDates(sourceRepo, variantRepo, tokenList, ct);
        if (!detached.divergenceDate.isEmpty()) {
            results.mainlineRepo = detached.mainlineRepo;
            results.variantRepo = detached.variantRepo;
            results.divergenceDate = detached.divergenceDate;
            if (results.cutOffDate.trimmed().isEmpty()) {
                results.cutOffDate = cutOffDate.trimmed().isEmpty()
                                         ? detached.cutOffDate : cutOffDate;
            }
        }
    }

    // Keep both repository identities canonical for the rest of the run.
    // Either side may have been renamed or transferred.
    if (!results.mainlineRepo.trimmed().isEmpty()) {
        this->sourceRepo = results.mainlineRepo.trimmed();
    }
    if (!results.variantRepo.trimmed().isEmpty()) {
        this->variantRepo = results.variantRepo.trimmed();
    }

    forkDate = results.forkDate;
    this->divergenceDate = results.divergenceDate;
    this->cutOffDate = results.cutOffDate;
    aheadBy = results.aheadBy;
    behindBy = results.behindBy;

    modernDay = results.cutOffDate;

    #ifdef QT_DEBUG
        qDebug().noquote()
            << QString("The divergence_date of the repository %1 is %2 and the cut_off_date is %3.")
                   .arg(this->variantRepo)
                   .arg(this->divergenceDate)
                   .arg(this->cutOffDate);

        qDebug().noquote() << "The variant2 is ==>";
        qDebug().noquote() << QString("\t Ahead by %1 commits").arg(aheadBy);
        qDebug().noquote() << QString("\t Behind by %1 commits").arg(behindBy);

        qDebug().noquote()
            << QString("Select an interval within the period [%1, %2] to limit the patches being checked.")
                   .arg(this->divergenceDate)
                   .arg(this->cutOffDate);
    #endif

    return results.divergenceDate;
}

QStringList GACPD::extractPatches(const QString& choosenDivergenceDate,
                                  const QString& choosenCutOffDate){
    divergenceDate = choosenDivergenceDate;
    cutOffDate = choosenCutOffDate;

    #ifdef QT_DEBUG
        qDebug()<<QString("Extracting Patches between %1 and %2 ...").arg(divergenceDate, cutOffDate);
    #endif
    // Long scans (thousands of merged PRs) take minutes, so report every page
    // to the progress dialog; the scan may finish before maxPages once it
    // passes PRs created after the cutoff.
    prPatches results = getPrPatches(
        sourceRepo, divergenceDate, cutOffDate, tokenList, ct,
        [this](int page, int maxPages, int matches) {
            emit stageChanged(
                QString("Finding all acceptable pull requests... page %1 of up to %2 "
                        "(%3 bug-fix PRs found)")
                    .arg(page).arg(maxPages).arg(matches));
        });

    QMap<QString, QString> dfData;

    for(int i = 0; i<results.prs.size(); i++){
        dfData[results.prs[i]] = results.title[i];
    }

    dfPatches.clear();

    for (int i = 0; i < results.prs.size(); i++) {
        PatchRow row;
        row.patchNumber = results.prs[i];
        row.patchTitle = results.title[i];
        dfPatches.append(row);
    }
    timers.append(results.seconds);
    return results.prs;
}

void GACPD::fetchPrData(){
    VariantShaExtractor results = getVariantSha(variantRepo,
                                                cutOffDate,
                                                tokenList,
                                                ct);

    if (results.variantSha.isEmpty())
        throw std::runtime_error("Could not resolve destination SHA; run is incomplete");

    prDataExtractor results2 = fetchPullRequestData(sourceRepo,
                                                    variantRepo,
                                                    pullRequests,
                                                    results.variantSha,
                                                    tokenList,
                                                    ct);
    timers.append(results2.runtimeSeconds);
    repoData = results2.pullRequestData;
}

gitFiles GACPD::getGitFiles(const QString& mainline,
                            const QString& baseSha,
                            const QString& prnum){
    gitFiles output;

    // Never call QDir::setCurrent() here. Classification can run in parallel,
    // and the process working directory is global to every thread. Changing it
    // from one PR can redirect another PR's relative Results/... paths.
    const QString mainlineRepoPath =
        QDir::cleanPath(repoClones + "/" + rootFolder + "/" + mainline);

    if (!QFileInfo(mainlineRepoPath).isDir()) {
        qWarning() << "Mainline clone directory does not exist for PR"
                   << prnum << ':' << mainlineRepoPath;
        return output;
    }

    if (baseSha.isEmpty()) {
        qWarning() << "Empty base SHA for PR" << prnum;
        return output;
    }

    QStringList fetchArgs;
    fetchArgs << "fetch"
              << "origin"
              << QString("pull/%1/head").arg(prnum);

    QProcess fetchProcess;
    fetchProcess.setWorkingDirectory(mainlineRepoPath);
    fetchProcess.start("git", fetchArgs);

    if (!fetchProcess.waitForFinished(-1) || fetchProcess.exitCode() != 0) {
        qWarning() << "git fetch failed for PR" << prnum
                   << fetchProcess.readAllStandardError();
        return output;
    }

    QStringList diffArgs;
    diffArgs << "diff"
             << "--name-status"
             << "--find-renames"
             << baseSha
             << "FETCH_HEAD";

    QProcess diffProcess;
    diffProcess.setWorkingDirectory(mainlineRepoPath);
    diffProcess.start("git", diffArgs);

    if (!diffProcess.waitForFinished(-1) || diffProcess.exitCode() != 0) {
        qWarning() << "git diff failed for PR" << prnum
                   << diffProcess.readAllStandardError();
        return output;
    }

    const QString stdoutText =
        QString::fromUtf8(diffProcess.readAllStandardOutput());

    const QStringList lines = stdoutText.split('\n', Qt::SkipEmptyParts);

    for (const QString &line : lines) {
        const QStringList parts = line.trimmed().split('\t');

        if (parts.isEmpty()) {
            continue;
        }

        if (parts[0].startsWith('R') && parts.size() == 3) {
            output.renamedFiles.insert(parts[1], parts[2]);
            output.renamedFileRecords.append(qMakePair(parts[1], parts[2]));
        } else if (parts[0] == "A" && parts.size() == 2) {
            output.addedFiles.append(parts[1]);
        }
    }

    return output;
}
