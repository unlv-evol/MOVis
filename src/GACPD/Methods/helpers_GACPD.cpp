#include "headers/GACPD/Methods/helpers_GACPD.h"
#include "headers/GACPD/Methods/appPath.h"
#include <QByteArray>
#include <QFile>

QStringList unique(const QStringList& ogList){
    QStringList uniqueKeywords;

    for (const QString &keyword : ogList) {
        if (!uniqueKeywords.contains(keyword)) {
            uniqueKeywords.append(keyword);
        }
    }

    return uniqueKeywords;
}

RepoDatesResults repoDates(const QString& repo,
                          const QStringList& tokenList,
                          int& ct){
    RepoDatesResults result;

    const QString url = QString("%1%2").arg(constant::GITHUB_API_BASE_URL, repo);

    try {
        if (tokenList.isEmpty()) {
            qCritical() << "No GitHub tokens are available for repository info:" << repo;
            return result;
        }

        const QJsonValue response = githubRestRequest(url, tokenList, ct);

        if (!response.isObject()) {
            qCritical() << "GitHub API request returned no repository data for:"
                        << repo << "URL:" << url;
            return result;
        }

        const QJsonObject obj = response.toObject();
        result.fullName = obj.value("full_name").toString().trimmed();
        if (result.fullName.isEmpty()) {
            result.fullName = repo.trimmed();
        }
        result.createdAt = obj.value("created_at").toString();
        result.updatedAt = obj.value("updated_at").toString();
        result.pushedAt = obj.value("pushed_at").toString();
        result.defaultBranch = obj.value("default_branch").toString();
    } catch (const std::exception& e) {
        qCritical() << url;
        qCritical() << "Error in function [repoDates]:" << e.what();
    } catch (...) {
        qCritical() << url;
        qCritical() << "Unknown error in function [repoDates]";
    }

    return result;
}

struct RepoCommitDateResult {
    QString sha;
    QString commitDate;
};

static RepoCommitDateResult repoCommitDateNew(const QString& repo,
                                              const QString& date,
                                              const QStringList& tokenList,
                                              int& ct,
                                              const QString& branch = QString()) {
    RepoCommitDateResult result;

    QString url = QString("%1%2/commits?until=%3&per_page=1")
                      .arg(constant::GITHUB_API_BASE_URL, repo, date);

    if (!branch.trimmed().isEmpty()) {
        url += QString("&sha=%1").arg(branch.trimmed());
    }

    try {
        if (tokenList.isEmpty()) {
            qCritical() << "No GitHub tokens are available for commit lookup:" << repo;
            return result;
        }

        const QJsonValue response = githubRestRequest(url, tokenList, ct);

        if (!response.isArray()) {
            qCritical() << "Unexpected GitHub commits response for:" << url << response;
            return result;
        }

        const QJsonArray commits = response.toArray();
        if (commits.isEmpty()) {
            qCritical() << "No commit found for" << repo
                        << "at or before" << date
                        << "on branch" << branch;
            return result;
        }

        const QJsonObject firstCommit = commits.first().toObject();
        result.sha = firstCommit.value("sha").toString();
        result.commitDate = firstCommit.value("commit")
                                .toObject()
                                .value("committer")
                                .toObject()
                                .value("date")
                                .toString();

        if (result.sha.isEmpty()) {
            qCritical() << "Commit lookup did not contain a SHA for:" << url;
        }
    } catch (const std::exception& e) {
        qCritical() << url;
        qCritical() << "Error in function [repoCommitDateNew]:" << e.what();
    } catch (...) {
        qCritical() << url;
        qCritical() << "Unknown error in function [repoCommitDateNew]";
    }

    return result;
}

RepoDateInfo getDivergenceDate(const QString& mainline,
                            const QString& variant,
                            const QStringList& tokenList,
                            int& ct,
                            QString leastDate = QString(),
                            QString divergeDate = QString()){
    RepoDateInfo result;
    int ahead = 0;
    int behind = 0;

    const RepoDatesResults mainlineInfo = repoDates(mainline, tokenList, ct);
    const RepoDatesResults variantInfo = repoDates(variant, tokenList, ct);

    // GitHub's REST repository endpoint follows repository rename/transfer
    // redirects. Preserve the canonical full_name returned by GitHub and use
    // it for every subsequent operation. This is important for GraphQL
    // repo:OWNER/REPO qualifiers and compare heads, which do not behave like
    // an HTTP URL redirect.
    const QString resolvedMainline =
        mainlineInfo.fullName.trimmed().isEmpty()
            ? mainline.trimmed()
            : mainlineInfo.fullName.trimmed();
    const QString resolvedVariant =
        variantInfo.fullName.trimmed().isEmpty()
            ? variant.trimmed()
            : variantInfo.fullName.trimmed();

    result.mainlineRepo = resolvedMainline;
    result.variantRepo = resolvedVariant;

    if (resolvedMainline != mainline.trimmed()) {
        qDebug().noquote() << QString("Resolved repository %1 -> %2")
                                 .arg(mainline, resolvedMainline);
    }
    if (resolvedVariant != variant.trimmed()) {
        qDebug().noquote() << QString("Resolved repository %1 -> %2")
                                 .arg(variant, resolvedVariant);
    }

    const QString mainlineBranch = mainlineInfo.defaultBranch.trimmed();
    const QString variantBranch = variantInfo.defaultBranch.trimmed();

    if (mainlineBranch.isEmpty() || variantBranch.isEmpty()) {
        qCritical() << "Unable to obtain default branches for divergence comparison:"
                    << mainline << "branch:" << mainlineBranch
                    << variant << "branch:" << variantBranch;

        result.forkDate.clear();
        result.divergenceDate.clear();
        result.cutOffDate = leastDate;
        result.aheadBy = ahead;
        result.behindBy = behind;
        return result;
    }

    // Match the Python implementation: when no cutoff is supplied, use the
    // older of the repositories' updated_at timestamps.
    if (leastDate.isEmpty()) {
        const QDateTime dateMl = QDateTime::fromString(mainlineInfo.updatedAt,
                                                       Qt::ISODate);
        const QDateTime dateVr = QDateTime::fromString(variantInfo.updatedAt,
                                                       Qt::ISODate);

        if (!dateMl.isValid() || !dateVr.isValid()) {
            qCritical() << "Unable to determine a cutoff date from repository metadata:"
                        << mainline << mainlineInfo.updatedAt
                        << variant << variantInfo.updatedAt;

            result.forkDate.clear();
            result.divergenceDate.clear();
            result.cutOffDate.clear();
            result.aheadBy = ahead;
            result.behindBy = behind;
            return result;
        }

        if (dateMl < dateVr) {
            leastDate = mainlineInfo.updatedAt;
        } else {
            leastDate = variantInfo.updatedAt;
        }
    }

    // Resolve each repository's default branch exactly as it existed at or
    // before the cutoff. This avoids comparing today's branch tips.
    const RepoCommitDateResult mainlineCommit =
        repoCommitDateNew(resolvedMainline,
                          leastDate,
                          tokenList,
                          ct,
                          mainlineBranch);

    const RepoCommitDateResult variantCommit =
        repoCommitDateNew(resolvedVariant,
                          leastDate,
                          tokenList,
                          ct,
                          variantBranch);

    if (mainlineCommit.sha.isEmpty() || variantCommit.sha.isEmpty()) {
        qCritical() << "Unable to resolve historical commits for divergence comparison:"
                    << mainline << mainlineCommit.sha
                    << variant << variantCommit.sha
                    << "cutoff:" << leastDate;

        result.forkDate.clear();
        result.divergenceDate.clear();
        result.cutOffDate = leastDate;
        result.aheadBy = ahead;
        result.behindBy = behind;
        return result;
    }

    const QString variantOwner = resolvedVariant.section('/', 0, 0).trimmed();
    if (variantOwner.isEmpty()) {
        qCritical() << "Unable to determine variant owner for:" << resolvedVariant;

        result.forkDate.clear();
        result.divergenceDate.clear();
        result.cutOffDate = leastDate;
        result.aheadBy = ahead;
        result.behindBy = behind;
        return result;
    }

    // Compare the two historical snapshots. This is equivalent to:
    // https://api.github.com/repos/{mainline}/compare/{sha_ml}...{variant_owner}:{sha_vr}
    const QString url = QString("%1%2/compare/%3...%4:%5")
                            .arg(constant::GITHUB_API_BASE_URL,
                                 resolvedMainline,
                                 mainlineCommit.sha,
                                 variantOwner,
                                 variantCommit.sha);

    try {
        if (tokenList.isEmpty()) {
            qCritical() << "No GitHub tokens are available for compare request:" << url;
        } else {
            const QJsonValue response = githubRestRequest(url, tokenList, ct);

            if (response.isObject()) {
                const QJsonObject obj = response.toObject();
                const QJsonObject mergeBase =
                    obj.value("merge_base_commit").toObject();

                if (divergeDate.isEmpty()) {
                    divergeDate = mergeBase.value("commit")
                                      .toObject()
                                      .value("committer")
                                      .toObject()
                                      .value("date")
                                      .toString();
                }

                ahead = obj.value("ahead_by").toInt();
                behind = obj.value("behind_by").toInt();

                if (divergeDate.isEmpty()) {
                    qCritical() << "GitHub compare response did not contain"
                                << "merge_base_commit.commit.committer.date for:"
                                << url;
                }
            } else {
                qCritical() << "GitHub comparison returned no data for:" << url;
            }
        }
    } catch (const std::exception& e) {
        qCritical() << url;
        qCritical() << "Error in function [getDivergenceDate]:" << e.what();
    } catch (...) {
        qCritical() << url;
        qCritical() << "Unknown error in function [getDivergenceDate]";
    }

    // Fork date is intentionally no longer obtained or used.
    result.forkDate.clear();
    result.divergenceDate = divergeDate;
    result.cutOffDate = leastDate;
    result.aheadBy = ahead;
    result.behindBy = behind;

    return result;
}

RepoDateInfo getDetachedPairDates(const QString& mainline,
                                  const QString& variant,
                                  const QStringList& tokenList,
                                  int& ct){
    RepoDateInfo result;
    result.aheadBy = 0;
    result.behindBy = 0;

    const RepoDatesResults mainlineInfo = repoDates(mainline, tokenList, ct);
    const RepoDatesResults variantInfo = repoDates(variant, tokenList, ct);

    result.mainlineRepo = mainlineInfo.fullName.trimmed().isEmpty()
                              ? mainline.trimmed() : mainlineInfo.fullName.trimmed();
    result.variantRepo = variantInfo.fullName.trimmed().isEmpty()
                             ? variant.trimmed() : variantInfo.fullName.trimmed();

    // repoDates falls back to the input name when the lookup fails, so check
    // the timestamps rather than full_name to know whether GitHub answered.
    const QDateTime mlCreated = QDateTime::fromString(mainlineInfo.createdAt, Qt::ISODate);
    const QDateTime vrCreated = QDateTime::fromString(variantInfo.createdAt, Qt::ISODate);
    const QDateTime mlUpdated = QDateTime::fromString(mainlineInfo.updatedAt, Qt::ISODate);
    const QDateTime vrUpdated = QDateTime::fromString(variantInfo.updatedAt, Qt::ISODate);

    if (!mlCreated.isValid() || !vrCreated.isValid() ||
        !mlUpdated.isValid() || !vrUpdated.isValid()) {
        qCritical() << "Unable to read repository metadata for detached pair:"
                    << mainline << variant;
        return result;
    }

    result.divergenceDate = (vrCreated < mlCreated ? mainlineInfo.createdAt
                                                   : variantInfo.createdAt);
    result.cutOffDate = (mlUpdated < vrUpdated ? mainlineInfo.updatedAt
                                               : variantInfo.updatedAt);

    qDebug().noquote()
        << QString("Detached pair %1 -> %2: estimated divergence %3, cut-off %4")
               .arg(result.mainlineRepo, result.variantRepo,
                    result.divergenceDate, result.cutOffDate);
    return result;
}

QString nodeExecutable(const QString& projRoot){
    Q_UNUSED(projRoot);
    // Node/npm is an immutable runtime dependency bundled with MOVis. The npm
    // working directory may be Application Support on macOS, but the actual
    // executable must still come from Contents/Resources/tools.
    const QString appDir = movisLocalRoot();

    #if defined(Q_OS_WIN)

    #if defined(Q_PROCESSOR_X86_64)
        return appDir + "/tools/node/windows/x64/npm.cmd";
    #elif defined(Q_PROCESSOR_ARM_64)
        return appDir + "/tools/node/windows/arm64/npm.cmd";
    #elif defined(Q_PROCESSOR_X86_32)
        return appDir + "/tools/node/windows/x86/npm.cmd";
    #else
        return QString();
    #endif

    #elif defined(Q_OS_MACOS)

    #if defined(Q_PROCESSOR_X86_64)
        return appDir + "/tools/node/macos/x64/bin/npm";
    #elif defined(Q_PROCESSOR_ARM_64)
        return appDir + "/tools/node/macos/arm64/bin/npm";
    #else
        return QString();
    #endif

    #elif defined(Q_OS_LINUX)

    #if defined(Q_PROCESSOR_X86_64)
        return appDir + "/tools/node/linux/x64/bin/npm";
    #elif defined(Q_PROCESSOR_ARM_64)
        return appDir + "/tools/node/linux/arm64/bin/npm";
    #elif defined(Q_PROCESSOR_ARM)
        return appDir + "/tools/node/linux/arm/bin/npm";
    #else
        return QString();
    #endif

    #else
        return QString();
    #endif
}

bool setupJscpd(const QString& projectRoot){
    QDir rootDir(projectRoot);

    if (!rootDir.exists()) {
        qDebug() << "Project root does not exist:" << projectRoot;
        return false;
    }

    const QString packageJsonPath = rootDir.filePath("package.json");
    const QString packageLockPath = rootDir.filePath("package-lock.json");

    if (!QFileInfo::exists(packageJsonPath)) {
        qDebug() << "package.json not found at:" << packageJsonPath;
        return false;
    }

    if (!QFileInfo::exists(packageLockPath)) {
        qDebug() << "package-lock.json not found at:" << packageLockPath;
        return false;
    }

    const QString npmPath = nodeExecutable(projectRoot);

    if (npmPath.isEmpty()) {
        qDebug() << "Unsupported OS or architecture.";
        return false;
    }

    if (!QFileInfo::exists(npmPath)) {
        qDebug() << "npm executable not found at:" << npmPath;
        return false;
    }

    QProcess npmProcess;
    npmProcess.setWorkingDirectory(projectRoot);

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

    const QString nodeBinDir = QFileInfo(npmPath).absolutePath();
    const QString oldPath = env.value("PATH");

    #ifdef Q_OS_WIN
        env.insert("PATH", nodeBinDir + ";" + oldPath);
        npmProcess.setProcessEnvironment(env);
    #else
        env.insert("PATH", nodeBinDir + ":" + oldPath);
        npmProcess.setProcessEnvironment(env);
    #endif

    QStringList npmArgs;
    // package-lock.json is part of the MOVis reproducibility contract. npm ci
    // installs exactly that dependency graph instead of resolving a newer
    // compatible JSCPD release.
    npmArgs << "ci" << "--no-audit" << "--no-fund";

    qDebug() << "Running npm ci in:" << projectRoot;
    qDebug() << "Using npm:" << npmPath;
    qDebug() << "Using node bin dir:" << nodeBinDir;
    qDebug() << "PATH for npm:" << env.value("PATH");

    npmProcess.start(npmPath, npmArgs);

    if (!npmProcess.waitForStarted()) {
        qDebug() << "Failed to start npm:" << npmProcess.errorString();
        return false;
    }

    npmProcess.waitForFinished(-1);

    const QString npmStdout = QString::fromUtf8(npmProcess.readAllStandardOutput());
    const QString npmStderr = QString::fromUtf8(npmProcess.readAllStandardError());

    qDebug() << "npm stdout:" << npmStdout;
    qDebug() << "npm stderr:" << npmStderr;

    if (npmProcess.exitStatus() != QProcess::NormalExit || npmProcess.exitCode() != 0) {
        qDebug() << "npm ci failed with exit code:" << npmProcess.exitCode();
        return false;
    }

    return true;
}

QString savePatch(const QString &storageDir,
                  const QString &fileName,
                  const QString &fileContent,
                  int& dupCount){
    QString patchPath;

    QDir dir(storageDir);

    if (!dir.exists()) {
        if (!QDir().mkpath(storageDir)) {
            throw std::runtime_error("Failed to create patch storage directory");
        }
    }

    const QString basePatchPath = storageDir + "/" + fileName + ".patch";
    patchPath = basePatchPath;

    // QIODevice::NewOnly intentionally refuses to overwrite existing data.
    // Choose a free filename first so rerunning a classification does not
    // turn an otherwise valid file into ERROR just because its patch exists.
    while (QFile::exists(patchPath)) {
        patchPath = storageDir + "/" + fileName + "_" +
                    QString::number(dupCount) + ".patch";
        ++dupCount;
    }

    QFile patchFile(patchPath);

    // Python open(..., "x") means create new file and fail if it exists.
    if (!patchFile.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
        throw std::runtime_error(
            QString("Failed to create patch file: %1").arg(patchPath).toStdString()
            );
    }

    const QByteArray patchBytes = fileContent.toUtf8();
    const qint64 written = patchFile.write(patchBytes);
    const bool flushed = patchFile.flush();
    patchFile.close();

    if (written != patchBytes.size() || !flushed) {
        QFile::remove(patchPath);
        throw std::runtime_error(
            QString("Failed to fully write patch file: %1").arg(patchPath).toStdString()
            );
    }

    // Verify exact persisted bytes so a partial/failed filesystem write cannot
    // silently become research input later in the classification pipeline.
    QFile verifyPatch(patchPath);
    if (!verifyPatch.open(QIODevice::ReadOnly)) {
        QFile::remove(patchPath);
        throw std::runtime_error(
            QString("Failed to reopen patch file for verification: %1")
                .arg(patchPath).toStdString()
            );
    }

    const QByteArray savedBytes = verifyPatch.readAll();
    verifyPatch.close();

    if (savedBytes != patchBytes) {
        QFile::remove(patchPath);
        throw std::runtime_error(
            QString("Patch file verification failed: %1").arg(patchPath).toStdString()
            );
    }

    qDebug()<<patchPath;
    return patchPath;
}

bool copyDirectory(const QString &sourceDirPath, const QString &destinationDirPath){
    QDir sourceDir(sourceDirPath);

    if (!sourceDir.exists()) {
        return false;
    }

    QDir destinationDir(destinationDirPath);

    if (!destinationDir.exists()) {
        if (!QDir().mkpath(destinationDirPath)) {
            return false;
        }
    }

    QFileInfoList entries = sourceDir.entryInfoList(
        QDir::NoDotAndDotDot | QDir::Files | QDir::Dirs
        );

    for (const QFileInfo &entry : entries) {
        QString srcPath = entry.absoluteFilePath();
        QString dstPath = destinationDir.filePath(entry.fileName());

        if (entry.isDir()) {
            if (!copyDirectory(srcPath, dstPath)) {
                return false;
            }
        } else {
            if (QFile::exists(dstPath)) {
                QFile::remove(dstPath);
            }

            if (!QFile::copy(srcPath, dstPath)) {
                return false;
            }
        }
    }

    return true;
}

QString file_Name(const QString &name){
    if (name.startsWith(".")) {
        return name.mid(1);
    }

    if (name.contains("/")) {
        return name.split("/").last();
    }

    if (!name.contains("/")) {
        return name;
    }

    throw std::runtime_error("Invalid file name");
}

QStringList file_Dir(const QString &name){
    if (name.startsWith(".")) {
        return QStringList{name.mid(1, 1)};
    }

    if (name.contains("/")) {
        QStringList parts = name.split("/", Qt::SkipEmptyParts);
        parts.removeLast();
        return parts;
    }

    if (!name.contains("/")) {
        return QStringList{};
    }

    throw std::runtime_error("Invalid file dir");
}
