#include "headers/helpers/resultFolders.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

// MyProject_001_2025-01-01_2025-06-30
static QRegularExpression sliceNamePattern(const QString &projectName)
{
    return QRegularExpression(
        QString("^%1_\\d{3}_\\d{4}-\\d{2}-\\d{2}_\\d{4}-\\d{2}-\\d{2}$")
            .arg(QRegularExpression::escape(projectName)));
}

// The same shape, for any project name.
static const QRegularExpression &anySliceNamePattern()
{
    static const QRegularExpression pattern(
        "^.+_\\d{3}_\\d{4}-\\d{2}-\\d{2}_\\d{4}-\\d{2}-\\d{2}$");
    return pattern;
}

// Sibling folders of `projectResultsPath` that are windows of that project.
static QFileInfoList sliceDirectories(const QString &projectResultsPath)
{
    const QFileInfo projectInfo(projectResultsPath);
    const QString projectName = projectInfo.fileName();
    QDir parent(projectInfo.absolutePath());

    if (!parent.exists() || projectName.isEmpty()) {
        return {};
    }

    const QRegularExpression pattern = sliceNamePattern(projectName);
    QFileInfoList matches;

    const QFileInfoList directories = parent.entryInfoList(
        QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);

    for (const QFileInfo &directory : directories) {
        if (pattern.match(directory.fileName()).hasMatch()) {
            matches.append(directory);
        }
    }

    return matches;
}

static QStringList absolutePathsOf(const QFileInfoList &directories)
{
    QStringList paths;
    paths.reserve(directories.size());
    for (const QFileInfo &directory : directories) {
        paths.append(directory.absoluteFilePath());
    }
    return paths;
}

QString sanitizedFolderName(QString name)
{
    return name.replace('/', '_').replace('-', '_').replace(':', '_');
}

QString sixMonthSliceName(const QString &projectName,
                          int windowIndex,
                          const DateWindow &window)
{
    return QString("%1_%2_%3_%4")
        .arg(projectName)
        .arg(windowIndex, 3, 10, QChar('0'))
        .arg(window.start.toString(Qt::ISODate))
        .arg(window.end.toString(Qt::ISODate));
}

bool projectHasResults(const QString &projectResultsPath)
{
    return QDir(projectResultsPath).exists()
        || !sliceDirectories(projectResultsPath).isEmpty();
}

QStringList resultWindowsUnder(const QString &selectedPath)
{
    const QFileInfo selectedInfo(selectedPath);

    // The user may have picked one window folder directly. Use it as-is rather
    // than asking again which window they meant.
    if (selectedInfo.exists()
        && selectedInfo.isDir()
        && anySliceNamePattern().match(selectedInfo.fileName()).hasMatch()) {
        return {selectedInfo.absoluteFilePath()};
    }

    // Result sets created before sibling window folders existed kept their
    // windows in subfolders named slice_*.
    QDir root(selectedPath);
    if (root.exists()) {
        const QFileInfoList legacySlices = root.entryInfoList(
            QStringList() << "slice_*",
            QDir::Dirs | QDir::NoDotAndDotDot,
            QDir::Name);

        if (!legacySlices.isEmpty()) {
            return absolutePathsOf(legacySlices);
        }
    }

    const QFileInfoList slices = sliceDirectories(selectedPath);
    if (slices.isEmpty()) {
        return {selectedPath};
    }

    return absolutePathsOf(slices);
}
