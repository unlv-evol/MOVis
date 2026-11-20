#ifndef LEGACY_REPORTS_H
#define LEGACY_REPORTS_H

#include <QHash>
#include <QJsonArray>
#include <QString>
#include "headers/structures/jscpdMatchKeys.h"

// Reconstructing jscpd evidence for result folders produced before MOVis
// preserved it.
//
// Current runs keep the exact report each hunk resolved at, including a
// separate 10-token context scan. Older result sets overwrote that file during
// later classification passes, so for those the viewer re-derives what it can:
// it reads clone presence back out of the surviving report, and re-runs jscpd
// in an isolated workspace to recover the context scan.
//
// None of this is needed for results produced by a current version.

// For each hunk_<n>_additions / _deletions file in the report, whether the
// clone it recorded really covers lines that the hunk changed.
QHash<QString, bool> classificationClonePresenceByAddDeleteHunk(
    const QJsonArray &duplicates,
    const QString &repoBaseAbs,
    const JscpdMatchKeys &keys,
    bool localWorkspaceReport);

// Re-runs the 10-token context scan in a temporary workspace holding only
// the hunk_<n>_context files and the selected comparison file. Returns false
// when jscpd or the bundled Node runtime is unavailable.
bool rebuildContextDuplicatesForLegacyResult(const QString &repoBaseAbs,
                                             const QString &rightPath,
                                             QJsonArray &duplicatesOut);

#endif // LEGACY_REPORTS_H
