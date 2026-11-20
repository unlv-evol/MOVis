#include "headers/helpers/tokenStore.h"

#include "headers/GACPD/Methods/appPath.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

QString tokensFilePath()
{
    return QDir(movisWorkingRoot()).filePath("tokens.txt");
}

QStringList loadTokensFile(const QStringList &fallback)
{
    // Older installs kept tokens.txt beside the executable rather than in the
    // writable working directory.
    QString path = tokensFilePath();
    if (!QFileInfo::exists(path)) {
        path = movisPath("tokens.txt");
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Could not open tokens.txt at:" << path
                 << "Error:" << file.errorString();
        return fallback;
    }

    QStringList tokens;
    QTextStream in(&file);

    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (!line.isEmpty()) {
            tokens.append(line);
        }
    }

    return tokens;
}

bool saveTokensFile(const QString &text, QString *errorOut)
{
    const QString path = tokensFilePath();

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorOut) {
            *errorOut = "Could not save tokens to:\n" + path;
        }
        return false;
    }

    QTextStream out(&file);
    out << text;
    return true;
}
