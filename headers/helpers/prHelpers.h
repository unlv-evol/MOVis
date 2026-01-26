#ifndef PRHELPERS_H
#define PRHELPERS_H

#include <QDir>
#include <QFileInfo>
#include <QString>
#include <QDirIterator>

bool parsePRClass(const QString& folderName, QString& prNumber, QString& prClassification);
void scanRootFolderForPRsAndClasses(QMap<QString, QSet<QString>>& classificationToPRs, const QString& rootPath);
QString normalizePrPath(const QString& basePath, const QString& filePath);
bool obtainRepositoryPath(const QString& baseFolder, QString& fileAbsolute, QString& finalRepoPath);
QStringList scanFilesUnderPath(const QString& baseFolder, const QString& mainFilePath);
QStringList scallAllFilesInCmp(const QString &root, const QString &pr, const QString &classification);
QString buildDirWithPRAndClassification(const QString &root, const QString &pr, const QString &cls);

#endif // PRHELPERS_H
