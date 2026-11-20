#ifndef JSCPD_EVIDENCE_H
#define JSCPD_EVIDENCE_H

#include <QHash>
#include <QJsonArray>
#include <QString>
#include "headers/GACPD/Structures/ClassificationTypes.h"

// Reading clone evidence back out of a jscpd report.
//
// A report lists duplicated ranges between pairs of files. What the classifier
// needs is narrower: for each generated hunk_<n>_additions / _deletions file,
// did jscpd find a clone that actually covers the lines the hunk changed?
// A clone that only covers untouched context around the change does not count.

// The bare file name out of a path jscpd reported.

// The bare file name out of a path jscpd reported.
QString jscpdReportedBaseName(QString reportedName);

// Keyed by check name: true when the clone jscpd recorded overlaps lines the
// hunk actually changed. A clone covering only untouched context is false.
QHash<QString, bool> directClonePresenceByCheck(
    const QJsonObject &data,
    const ClassificationWorkspace &workspace);

#endif // JSCPD_EVIDENCE_H
