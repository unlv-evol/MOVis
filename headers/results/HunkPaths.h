#ifndef HUNK_PATHS_H
#define HUNK_PATHS_H

#include <QString>
#include "headers/structures/jscpdMatchKeys.h"

// Matching the file paths jscpd reports against the files the viewer shows.
//
// jscpd reports whatever path it was handed, which varies with the version
// that produced the report and with whether the scan ran in the repository or
// in an isolated workspace. The viewer, meanwhile, displays generated hunk
// files named hunk_<n>_additions / hunk_<n>_deletions / hunk_<n>_context.
// Everything here bridges those two naming worlds.

// True when a reported path refers to the file shown in the right pane.
bool reportedPathMatchesRight(const QString &reportedName,
                              const JscpdMatchKeys &keys);

// "additions", "deletions" or "context" for a reported hunk file, empty for
// anything else.
QString reportedHunkKind(const QString &reportedName);

// The hunk file name the left pane displays for a reported path, folded to
// lower case for lookups. Empty when the path is not a hunk file.
QString displayedHunkKey(const QString &reportedName);

// A hunk_<n>_context file covers both sides of its hunk, so it maps onto
// two displayed files. Returns false for anything that is not a context file.
bool contextHunkDisplayKeys(const QString &reportedName,
                            QString &additionsKey,
                            QString &deletionsKey);

// Line numbers inside a generated hunk file are offset from the ones the
// left pane shows; this is how far to shift them.
int displayedHunkLineOffset(const QString &reportedName);

#endif // HUNK_PATHS_H
