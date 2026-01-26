#include "headers/helpers/fileHelpers.h"

QString canonicalOrOriginal(const QString& filePath){
    QFileInfo fileInfo(filePath);

    const QString truePath = fileInfo.canonicalFilePath();

    if(truePath.isEmpty())
        return filePath;

    return truePath;
}

bool hasExtension(const QString& path, QString extension){
    return path.endsWith(extension, Qt::CaseInsensitive);
}

QString normalizePath(const QString& pathIn){
    QString path = pathIn;
    path.replace('\\', '/');
    if (path.startsWith("./")) {
        QString tmp = path;
        tmp.remove(0, 2);
        path = tmp;
    }
    return QDir::cleanPath(path);
}

QString normSlashes(QString s){
    #ifdef Q_OS_WIN
        s.replace('\\', '/');
    #endif
        return s;
}

bool isUnder(const QString &parentFolder, const QString &childFolder) {
    QDir parent(parentFolder);
    const QString rel = parent.relativeFilePath(childFolder);
    return !rel.startsWith("..");
}

bool editorHasText(QPlainTextEdit* edit){
    if (!edit)
        return false;

    if (!edit->document())
        return false;

    return edit->document()->characterCount() > 1;
}
