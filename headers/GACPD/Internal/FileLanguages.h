#ifndef FILE_LANGUAGES_H
#define FILE_LANGUAGES_H

#include <QJsonObject>
#include <QString>
#include "headers/GACPD/Structures/ClassificationTypes.h"

// Working out what language a file is written in, and whether jscpd can scan
// it.
//
// MOVis keeps its own extension tables rather than asking jscpd, because the
// answer decides three separate things: whether a file is classified at all,
// which --format jscpd is invoked with, and what language is recorded in the
// result JSON.

// Extension, language and jscpd support for one file path.
FileExtensionInfo fileExtensionInfoFromInternalLists(const QString &file);

// Adds the file's extension and language to a result record.
void addFileLanguageMetadata(QJsonObject &object, const QString &file);

// The extension to give the copy handed to jscpd. Files matched by exact
// name rather than by suffix get the name itself.
QString jscpdExtensionFor(const FileExtensionInfo &extInfo);

// True when the mapping matched a whole file name, such as "Makefile".
bool isExactFileNameMapping(const FileExtensionInfo &extInfo);

// The jscpd --format name for this file, empty when jscpd has none.
QString registeredFormatFor(const FileExtensionInfo &extInfo);

#endif // FILE_LANGUAGES_H
