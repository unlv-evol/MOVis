#include "headers/helpers/dependencySetup.h"

#include "headers/GACPD/Methods/appPath.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

QString pinnedJscpdVersion()
{
    return QStringLiteral("5.0.16");
}

QString jscpdExecutableName()
{
#ifdef Q_OS_WIN
    return QStringLiteral("jscpd.cmd");
#else
    return QStringLiteral("jscpd");
#endif
}

bool hasPinnedJscpd(const QString &root)
{
    const QString binary =
        QDir(root).filePath("node_modules/.bin/" + jscpdExecutableName());
    if (!QFileInfo::exists(binary)) {
        return false;
    }

    QFile packageFile(QDir(root).filePath("node_modules/jscpd/package.json"));
    if (!packageFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    QJsonParseError error;
    const QJsonDocument document =
        QJsonDocument::fromJson(packageFile.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return false;
    }

    return document.object().value("version").toString() == pinnedJscpdVersion();
}

QString dependencyInstallRoot()
{
#ifdef Q_OS_MACOS
    return movisDependencyRoot();
#else
    return movisLocalRoot();
#endif
}
