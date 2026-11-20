// GACPD stage 4: following files through the repository history.
//
// A file that was renamed between the divergence point and the pull request
// has to be matched to its counterpart under the other name, so the run keeps
// a local clone of each repository and walks its rename history. Renames can
// form cycles when two files swap names, which is detected here rather than
// followed forever.

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
#include <exception>
#include <stdexcept>

void GACPD::ensureGitFoldersExists(){
    // Divergent repo path
    const QStringList divergentParts =
        variantRepo.split("/", Qt::SkipEmptyParts);

    if (divergentParts.isEmpty()) {
        #ifdef QT_DEBUG
            qDebug() << "Invalid divergent repo:" << variantRepo;
        #endif
        return;
    }

    const QString divergentRepoName = divergentParts[0];

    const QString divergentPath = movisWorkingPath(
        QString("Results/Repos_files/%1/%2")
            .arg(rootFolder, divergentRepoName));

    // Main-line repo path
    const QStringList mainParts =
        sourceRepo.split("/", Qt::SkipEmptyParts);

    if (mainParts.isEmpty()) {
        #ifdef QT_DEBUG
            qDebug() << "Invalid mainline repo:" << sourceRepo;
        #endif
        return;
    }

    const QString mainRepoName = mainParts[0];

    const QString mainPath = movisWorkingPath(
        QString("%1/%2/%3")
            .arg(repoClones, rootFolder, mainRepoName));

    // Create only if missing
    emit stageChanged(QString("Cloning Repository: %1").arg(variantRepo));
    if (!QFileInfo(divergentPath).isDir()) {
        const QString file = rootFolder + "/" + variantRepo;
        createGitFolder("Results/Repos_files", file);
    }

    emit stageChanged(QString("Cloning Repository: %1").arg(sourceRepo));
    if (!QFileInfo(mainPath).isDir()) {
        const QString file = rootFolder + "/" + sourceRepo;
        createGitFolder(repoClones, file);
    }
}

void GACPD::removeGitFolder(const QString &folder){
    QString folderName = folder;
    folderName.replace("\\", "/");

    if (!QFileInfo(folderName).isAbsolute()) {
        folderName = movisWorkingPath(folderName);
    }

    QDir dir(folderName);

    if (dir.exists()) {
        dir.removeRecursively();
    }
}

static bool runGit(const QString &cwd, const QStringList &args) {
    QProcess git;
    git.setWorkingDirectory(cwd);
    git.start("git", args);
    return git.waitForFinished(-1) && git.exitStatus() == QProcess::NormalExit
           && git.exitCode() == 0;
}

void GACPD::createGitFolder(const QString &folder,
                     const QString &repoName){
    QString normalizedRepoName = repoName;
    normalizedRepoName.replace("\\", "/");

    const QStringList repoOwner = normalizedRepoName.split("/", Qt::SkipEmptyParts);

    if (repoOwner.size() < 3) {
        #ifdef QT_DEBUG
            qDebug() << "Invalid repo name:" << repoName;
        #endif
        return;
    }

    QString baseFolder = folder;
    if (!QFileInfo(baseFolder).isAbsolute()) {
        baseFolder = movisWorkingPath(baseFolder);
    }

    const QString targetPath =
        QDir(baseFolder).filePath(repoOwner[0] + "/" + repoOwner[1]);

    QDir dir;

    if (!dir.exists(targetPath)) {
        if (!dir.mkpath(targetPath)) {
            #ifdef QT_DEBUG
                qDebug() << "Failed to create folder:" << targetPath;
            #endif
            return;
        }
    }

    QProcess longPathsProcess;
    QStringList longPathsArgs;
    longPathsArgs << "config"
                  << "--global"
                  << "core.longpaths"
                  << "true";

    longPathsProcess.start("git", longPathsArgs);
    longPathsProcess.waitForFinished(-1);

    const QString remoteUrl = QString("https://github.com/%1/%2")
                                  .arg(repoOwner[1], repoOwner[2]);

    // Skip-repository-deletion mode: earlier runs left their clones in place,
    // so seed this one from a local copy (hard-linked objects, no network
    // transfer) and then catch it up with GitHub.
    if (skipRepoDeletion) {
        const QString reusable = findReusableClone(repoOwner[0], repoOwner[1], repoOwner[2]);
        if (!reusable.isEmpty()) {
            const QString clonePath = QDir(targetPath).filePath(repoOwner[2]);
            if (runGit(targetPath, {"clone", "--local", reusable, repoOwner[2]})) {
                // Classification fetches pull/<n>/head from origin, so origin
                // must point back at GitHub rather than the local source.
                runGit(clonePath, {"remote", "set-url", "origin", remoteUrl});
                if (!runGit(clonePath, {"pull", "--ff-only"})) {
                    qWarning() << "Could not update reused clone from GitHub:" << clonePath;
                }
                qDebug() << "Reused local clone" << reusable << "->" << clonePath;
                return;
            }

            qWarning() << "Local clone reuse failed, cloning from GitHub:" << reusable;
            QDir(clonePath).removeRecursively();
        }
    }

    QProcess cloneProcess;
    cloneProcess.setWorkingDirectory(targetPath);

    QStringList cloneArgs;
    cloneArgs << "clone" << remoteUrl;

    cloneProcess.start("git", cloneArgs);
    cloneProcess.waitForFinished(-1);

    if (cloneProcess.exitCode() != 0) {
        #ifdef QT_DEBUG
            qDebug() << "git clone failed:";
            qDebug() << cloneProcess.readAllStandardError();
        #endif
    }
}

QString GACPD::findReusableClone(const QString &currentRoot,
                                 const QString &owner,
                                 const QString &repo) const {
    // Clones live at Results/Repos_{files,clones}/<runRoot>/<owner>/<repo>.
    // Any other run folder holding the same repository can seed this one.
    const QStringList cacheRoots = {
        movisWorkingPath("Results/Repos_clones"),
        movisWorkingPath("Results/Repos_files"),
    };

    for (const QString &cacheRoot : cacheRoots) {
        const QFileInfoList runRoots =
            QDir(cacheRoot).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QFileInfo &runRoot : runRoots) {
            if (runRoot.fileName() == currentRoot) {
                continue;
            }
            const QString candidate =
                QDir(runRoot.absoluteFilePath()).filePath(owner + "/" + repo);
            if (QFileInfo(candidate + "/.git").isDir()) {
                return candidate;
            }
        }
    }
    return QString();
}

void GACPD::obtainGitInformation(const QStringList &tokenList, int &ct){
    QString repoName = movisWorkingPath(
        "Results/Repos_files/" + rootFolder + "/" + variantRepo);

    const QString url =
        QString("https://api.github.com/repos/%1").arg(variantRepo);

    QString createdAt;

    QJsonValue response = githubRestRequest(url, tokenList, ct);

    if (response.isObject()) {
        QJsonObject data = response.toObject();

        createdAt = data.value("created_at").toString();

        if (!createdAt.isEmpty()) {
            #ifdef QT_DEBUG
                qDebug() << "Repository created at:" << createdAt;
            #endif
        } else {
            #ifdef QT_DEBUG
                qDebug() << "The 'created_at' field is not found in the response.";
            #endif
        }
    } else {
        #ifdef QT_DEBUG
            qDebug() << "GitHub API response was not a JSON object.";
        #endif
    }

    renames = obtainGitRenameHistory(createdAt, modernDay, repoName);

    repoName = movisWorkingPath(
        "Results/Repos_clones/" + rootFolder + "/" + sourceRepo);

    renamesMainline = obtainGitRenameHistory(createdAt, modernDay, repoName);

    cycles = findRenameCycles(renames);
}

void GACPD::getGitInformation(const QStringList &tokenList, int &ct){
    QStringList divergentParts = variantRepo.split("/", Qt::SkipEmptyParts);

    if (!divergentParts.isEmpty()) {
        QString repoName = divergentParts[0];

        removeGitFolder(
            "Results/Repos_files/" + rootFolder + "/" + repoName
            );
    }

    QStringList mainlineParts = sourceRepo.split("/", Qt::SkipEmptyParts);

    if (!mainlineParts.isEmpty()) {
        QString repoName = mainlineParts[0];

        removeGitFolder(
            repoClones + "/" + rootFolder + "/" + repoName
            );
    }

    QString file = rootFolder + "/" + variantRepo;
    createGitFolder("Results/Repos_files", file);

    file = rootFolder + "/" + sourceRepo;
    createGitFolder(repoClones, file);

    obtainGitInformation(tokenList, ct);
}

QMap<QString, QString> GACPD::obtainGitRenameHistory(const QString &startDate,
                                              const QString &endDate,
                                              const QString &repoPath){
    QMap<QString, QString> renameHistory;

    QProcess process;
    process.setWorkingDirectory(repoPath);

    QStringList args;
    args << "log"
         << "--diff-filter=R"
         << "--name-status"
         << "--pretty=format:"
         << QString("--since=%1").arg(startDate)
         << QString("--until=%1").arg(endDate)
         << "--reverse";

    process.start("git", args);

    if (!process.waitForFinished(-1)) {
        #ifdef QT_DEBUG
            qDebug() << "git log failed to finish.";
        #endif
        return renameHistory;
    }

    if (process.exitCode() != 0) {
        #ifdef QT_DEBUG
            qDebug() << "git log failed:";
            qDebug() << process.readAllStandardError();
        #endif
        return renameHistory;
    }

    const QString output = QString::fromUtf8(process.readAllStandardOutput());
    const QStringList lines = output.split('\n', Qt::SkipEmptyParts);

    for (const QString &line : lines) {
        const QStringList parts = line.trimmed().split('\t');

        if (parts.size() == 3 && parts[0].startsWith("R")) {
            const QString oldName = parts[1];
            const QString newName = parts[2];

            renameHistory.insert(oldName, newName);
        }
    }

    return renameHistory;
}

QString GACPD::traceBackToOrigin(QString file,
                          const QMap<QString, QString> &renames){
    QMap<QString, QString> reverseMap;

    // Python equivalent:
    // reverse_map = {v: k for k, v in renames.items()}
    for (QMap<QString, QString>::const_iterator it = renames.constBegin(); it != renames.constEnd(); it++) {
        reverseMap.insert(it.value(), it.key());
    }

    // Historical rename maps can contain cycles when a path is reused later
    // (for example A -> B and, years later, B -> A). The old unbounded loop
    // would never return for such repositories. This became visible for
    // rename-only PRs once PR rename metadata started being resolved through
    // the historical mainline map.
    QSet<QString> visited;
    while (reverseMap.contains(file)) {
        if (visited.contains(file)) {
            qWarning() << "Rename cycle detected while tracing backward at"
                       << file << "- stopping at the last stable path";
            break;
        }

        visited.insert(file);
        const QString previous = reverseMap.value(file);
        if (previous.isEmpty() || previous == file) {
            break;
        }
        file = previous;
    }

    return file;
}

QString GACPD::traceForwardToLatest(QString file,
                             const QMap<QString, QString> &renames){
    // Use the same cycle protection in the divergent history. A rename-only
    // PR must never be able to hold an entire PR worker at 0/N files merely
    // because a historical path was reused.
    QSet<QString> visited;
    while (renames.contains(file)) {
        if (visited.contains(file)) {
            qWarning() << "Rename cycle detected while tracing forward at"
                       << file << "- stopping at the last stable path";
            break;
        }

        visited.insert(file);
        const QString next = renames.value(file);
        if (next.isEmpty() || next == file) {
            break;
        }
        file = next;
    }

    return file;
}

QPair<QString, QString> GACPD::getRenamePath(const QString &filename,
                                      const QMap<QString, QString> &mainline,
                                      const QMap<QString, QString> &divergent){
    const QString origin = traceBackToOrigin(filename, mainline);
    const QString latest = traceForwardToLatest(origin, divergent);

    return qMakePair(origin, latest);
}

CycleSearchResult GACPD::findCycleFromFile(const QString &start,
                                    const QMap<QString, QString> &renames,
                                    const QSet<QString> &visited){
    CycleSearchResult result;

    QStringList path;
    QMap<QString, int> localVisited;

    QString current = start;

    while (renames.contains(current)) {
        if (localVisited.contains(current)) {
            const int cycleStartIndex = localVisited.value(current);

            for (int i = cycleStartIndex; i < path.size(); i++) {
                result.cycle.append(path[i]);
            }

            result.cycle.append(current);
            result.found = true;

            return result;
        }

        if (visited.contains(current)) {
            return result;
        }

        localVisited.insert(current, path.size());
        path.append(current);

        current = renames.value(current);
    }

    return result;
}

QVector<QStringList> GACPD::findRenameCycles(const QMap<QString, QString> &renames){
    QSet<QString> visited;
    QVector<QStringList> cycles;

    for (QMap<QString, QString>::const_iterator it = renames.constBegin(); it != renames.constEnd(); it++) {
        const QString file = it.key();

        if (!visited.contains(file)) {
            CycleSearchResult result =
                findCycleFromFile(file, renames, visited);

            if (result.found) {
                cycles.append(result.cycle);

                for (const QString &cycleFile : result.cycle) {
                    visited.insert(cycleFile);
                }
            }
        }
    }

    return cycles;
}
