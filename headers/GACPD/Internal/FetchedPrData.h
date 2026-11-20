#ifndef FETCHED_PR_DATA_H
#define FETCHED_PR_DATA_H

#include <QString>
#include <QVariantMap>
#include "headers/GACPD/Structures/ClassificationTypes.h"

// Reading the loosely-typed maps that fetchPullRequestData produces.
//
// Pull request data is carried around as QVariantMaps because it is assembled
// straight from GitHub's JSON. These accessors are the one place that knows
// which keys mean what, and how older captures spelled them.

// Whether GitHub actually supplied patch text for this file.
bool fetchedPatchAvailable(const QVariantMap &entry);

// Why the patch text is missing, when it is.
QString fetchedPatchStatus(const QVariantMap &entry);

// The added and renamed files recorded for one pull request.
gitFiles gitFilesFromFetchedPrData(const QVariantMap &prData);

#endif // FETCHED_PR_DATA_H
