#include "headers/GACPD/Methods/appPath.h"

#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>
#include <QFile>
#include <QDateTime>

#ifdef Q_OS_MACOS
static QDir macContentsDir()
{
    // QCoreApplication::applicationDirPath() inside a bundle is:
    //   /path/to/MOVis.app/Contents/MacOS
    QDir dir(QCoreApplication::applicationDirPath());
    dir.cdUp(); // Contents
    return dir;
}
#endif

QString movisLocalRoot()
{
#ifdef Q_OS_MACOS
    // Keep runtime/support data inside the application package rather than
    // beside MOVis.app. qmake copies package.json, tools, assets, etc. here.
    const QDir contents = macContentsDir();
    return QDir::cleanPath(contents.filePath("Resources"));
#else
    return QDir::cleanPath(QCoreApplication::applicationDirPath());
#endif
}

QString movisExternalRoot()
{
#ifdef Q_OS_MACOS
    QDir dir = macContentsDir();
    dir.cdUp(); // MOVis.app
    dir.cdUp(); // folder containing MOVis.app
    return QDir::cleanPath(dir.absolutePath());
#else
    return movisLocalRoot();
#endif
}


QString movisWorkingRoot()
{
#ifdef Q_OS_MACOS
    QString root =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

    if (root.isEmpty()) {
        root = QDir(QDir::homePath()).filePath(
            "Library/Application Support/MOVis");
    }

    return QDir::cleanPath(root);
#else
    return movisLocalRoot();
#endif
}

QString movisDependencyRoot()
{
#ifdef Q_OS_MACOS
    return QDir::cleanPath(
        QDir(movisWorkingRoot()).filePath("Dependencies"));
#else
    return movisLocalRoot();
#endif
}

QString movisWorkingPath(const QString &relativePath)
{
    if (relativePath.isEmpty()) {
        return movisWorkingRoot();
    }

    return QDir::cleanPath(
        QDir(movisWorkingRoot()).filePath(relativePath));
}

QString movisResultsRoot()
{
#ifdef Q_OS_MACOS
    // Keep public results outside the .app, but do NOT place them in
    // Documents/Desktop/Downloads. Those locations are protected by macOS
    // Files & Folders privacy (TCC), so a standalone MOVis.app can be denied
    // access even when the exact same build works from Qt Creator/Terminal.
    // The user's home directory is writable without that per-folder privacy
    // grant and is still easy to access in Finder:
    //     ~/MOVis/Results/Repos_results
    return QDir::cleanPath(
        QDir(QDir::homePath()).filePath("MOVis/Results/Repos_results"));
#else
    return QDir::cleanPath(
        QDir(movisLocalRoot()).filePath("Results/Repos_results"));
#endif
}

QString movisPath(const QString &relativePath)
{
    return QDir::cleanPath(QDir(movisLocalRoot()).filePath(relativePath));
}

QString movisResultsPath(const QString &relativePath)
{
    if (relativePath.isEmpty()) {
        return movisResultsRoot();
    }

    return QDir::cleanPath(QDir(movisResultsRoot()).filePath(relativePath));
}

bool movisEnsureLocalRoot()
{
    return QDir().mkpath(movisLocalRoot());
}

bool movisEnsureWorkingRoot()
{
    const QString root = movisWorkingRoot();

    return QDir().mkpath(root) &&
           QDir().mkpath(QDir(root).filePath("Results/Repos_files")) &&
           QDir().mkpath(QDir(root).filePath("Results/Repos_clones"));
}

bool movisEnsureResultsRoot()
{
    const QString root = movisResultsRoot();
    return QDir().mkpath(root);
}

bool movisTestWritable(const QString &directoryPath, QString *errorMessage)
{
    if (directoryPath.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Directory path is empty.");
        }
        return false;
    }

    if (!QDir().mkpath(directoryPath)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Could not create directory: %1")
                                .arg(directoryPath);
        }
        return false;
    }

    const QString probeName = QStringLiteral(".movis_write_test_%1_%2")
        .arg(QCoreApplication::applicationPid())
        .arg(QDateTime::currentMSecsSinceEpoch());
    const QString probePath = QDir(directoryPath).filePath(probeName);

    QFile probe(probePath);
    if (!probe.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Cannot write to %1: %2")
                                .arg(directoryPath, probe.errorString());
        }
        return false;
    }

    const qint64 written = probe.write("MOVis write test");
    const QString writeError = probe.errorString();
    probe.close();
    QFile::remove(probePath);

    if (written < 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Write test failed in %1: %2")
                                .arg(directoryPath, writeError);
        }
        return false;
    }

    return true;
}
