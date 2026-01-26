#ifndef FILEHELPERS_H
#define FILEHELPERS_H

#include <QDir>
#include <QString>
#include <QFileInfo>
#include <QPlainTextEdit>

QString canonicalOrOriginal(const QString& filePath);
bool hasExtension(const QString& path, QString extension);
QString normalizePath(const QString& pathIn);
bool isUnder(const QString &parentFolder, const QString &childFolder);
bool editorHasText(QPlainTextEdit* edit);
QString normSlashes(QString s);

#endif // FILEHELPERS_H
