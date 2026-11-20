#ifndef RESULT_RECORDS_H
#define RESULT_RECORDS_H

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include "headers/GACPD/Structures/ClassificationTypes.h"

// Building the JSON records a classification run writes out.
//
// Two shapes are produced: one results.json per classified file, holding
// everything known about that file, and one compact entry per file inside the
// pull request's own pr_results.json.

// The full per-file record.

// The full per-file record, written to <file>/results.json.
bool writeIndividualFileResultJson(
    const QString &path,
    const QString &fileName,
    const QString &prNr,
    const QString &mainline,
    const QString &divergentRepo,
    const QString &changeStatus,
    bool renamed,
    const QString &renamedFileName,
    const QString &divergentPath,
    const QString &divergentPathType,
    const QString &classification,
    bool patchAvailable,
    const QString &patchStatus,
    const QStringList &similarityLines = QStringList(),
    const QVector<HunkClassificationInfo> &hunkClassifications = {},
    const QString &errorReason = QString());

// The compact entry that goes into the pull request's own results file.
QJsonObject makePrFileResult(const QString &fileName,
                             const QString &classification,
                             bool patchAvailable,
                             const QString &patchStatus,
                             const QStringList &similarityLines = QStringList(),
                             const QVector<HunkClassificationInfo> &hunkClassifications = {},
                             const QString &errorReason = QString());

#endif // RESULT_RECORDS_H
