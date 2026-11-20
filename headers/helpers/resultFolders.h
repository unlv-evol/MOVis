#ifndef RESULT_FOLDERS_H
#define RESULT_FOLDERS_H

#include <QString>
#include <QStringList>

#include "dateWindows.h"

// Naming and locating the folders a classification run writes into.
//
// A normal run that fits in one six-month window keeps the legacy layout, a
// single folder named after the project:
//
//     Repos_results/MyProject/
//
// A longer run writes one sibling folder per window instead:
//
//     Repos_results/MyProject_001_2025-01-01_2025-06-30/
//     Repos_results/MyProject_002_2025-07-01_2025-12-31/

// Strips the characters repository names and ISO timestamps contribute that
// cannot appear in a folder name.
QString sanitizedFolderName(QString name);

// Folder name for one window of a multi-window normal run.
QString sixMonthSliceName(const QString &projectName,
                          int windowIndex,
                          const DateWindow &window);

// True when a run has already written results under this project name, in
// either layout. Used to keep project names unique.
bool projectHasResults(const QString &projectResultsPath);

// Result folders the user should be offered for a directory they picked. One
// entry means open it directly; several means ask which window they want.
QStringList resultWindowsUnder(const QString &selectedPath);

#endif // RESULT_FOLDERS_H
