#include "headers/helpers/prHelpers.h"
#include "headers/helpers/fileHelpers.h"

bool parsePRClass(const QString& folderName, QString& prNumber, QString& prClassification){
    int lastUnderscore = folderName.lastIndexOf('_');

    if (lastUnderscore <= 0 || lastUnderscore >= folderName.size() - 1)
        return false;

    prNumber = folderName.left(lastUnderscore);
    prClassification = folderName.mid(lastUnderscore + 1);
    return !prNumber.isEmpty() && !prClassification.isEmpty();
}

void scanRootFolderForPRsAndClasses(QMap<QString, QSet<QString>>& classificationToPRs, const QString &rootPath) {
    classificationToPRs.clear();
    QDir root(rootPath);

    if (!root.exists())
        return;

    const QList<QFileInfo> entries = root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);

    for(const QFileInfo& fileInfo: entries){
        QString pr;
        QString classification;
        if (parsePRClass(fileInfo.fileName(), pr, classification)) {
            if(classification == "MO")
                classificationToPRs[classification].insert(pr);
        }
    }
}

QString normalizePrPath(const QString &basePath, const QString &filePath) {
    QDir baseDir(basePath);
    QString rel = baseDir.relativeFilePath(filePath);

    #ifdef Q_OS_WIN
        rel.replace('\\', '/');
    #endif

    if (rel.isEmpty()) {
        QFileInfo fi(filePath);
        rel = fi.fileName();
    }
    return rel;
}

bool obtainRepositoryPath(const QString& baseFolder, QString& fileAbsolute, QString& finalRepoPath) {
    QString norm = fileAbsolute;

    #ifdef Q_OS_WIN
        norm.replace('\\', '/');
    #endif

    int pos = norm.indexOf(baseFolder+"/");
    if (pos < 0) {
        pos = norm.lastIndexOf(baseFolder);
        if (pos < 0)
            return false;
    }
    finalRepoPath = norm.left(pos); // up to .../<Hunk>/<Github_Path>
    return !finalRepoPath.isEmpty();
}

QStringList scanFilesUnderPath(const QString& baseFolder, const QString& mainFilePath) {
    QStringList out;
    const QString srcAbs = QDir(mainFilePath).filePath(baseFolder);
    QDir srcDir(srcAbs);

    if (!srcDir.exists())
        return out;

    QDirIterator it(srcAbs, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QString f = canonicalOrOriginal(it.next());
        if (hasExtension(f, ".patch"))
            continue;
        out.append(f);
    }
    out.sort(Qt::CaseInsensitive);
    return out;
}

QString buildDirWithPRAndClassification(const QString &root, const QString &pr, const QString &cls) {
    return QDir(root).filePath(pr + "_" + cls);
}

QStringList scallAllFilesInCmp(const QString &rootDir,
                                     const QString &pullRequestId,
                                     const QString &classification)
{
    QStringList files;

    const QString baseAbsDir = buildDirWithPRAndClassification(rootDir, pullRequestId, classification);
    const QDir baseDir(baseAbsDir);
    if (!baseDir.exists())
        return files;

    // Under baseAbsDir: directories representing "hunks"
    const QList<QFileInfo> hunkDirs =
        baseDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);

    for (const QFileInfo &hunkDirInfo : hunkDirs) {
        const QDir hunkDir(hunkDirInfo.absoluteFilePath());

        // Under each hunk dir: directories representing repos
        const QList<QFileInfo> repoDirs =
            hunkDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);

        for (const QFileInfo &repoDirInfo : repoDirs) {
            // Each repo has a "cmp/" directory we care about
            const QString cmpAbsDir = QDir(repoDirInfo.absoluteFilePath()).filePath("cmp");
            const QDir cmpDir(cmpAbsDir);
            if (!cmpDir.exists())
                continue;

            // Walk all files under cmp/ recursively
            QDirIterator fileIt(cmpAbsDir, QDir::Files, QDirIterator::Subdirectories);
            while (fileIt.hasNext()) {
                const QString fileAbsPath = canonicalOrOriginal(fileIt.next());

                // Skip patch files
                if (hasExtension(fileAbsPath, ".patch"))
                    continue;

                files.append(fileAbsPath);
            }
        }
    }

    files.sort(Qt::CaseInsensitive);
    return files;
}
