#include "headers/GACPD/Internal/FetchedPrData.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>
#include <QVariantList>

bool fetchedPatchAvailable(const QVariantMap &entry){
    // GitHub's changes == 0 is authoritative: there is no textual diff to
    // classify (notably "File renamed without changes"). Never let an old
    // cached empty "patch" key turn that case back into patch processing.
    if (entry.value("changes").toInt() == 0) {
        return false;
    }

    const QString patchText = entry.value("patch").toString();

    if (entry.contains("patch_available")) {
        return entry.value("patch_available").toBool() &&
               !patchText.isEmpty();
    }

    // Backward-compatible fallback for data produced by older builds. Those
    // builds always inserted a "patch" key, even when GitHub omitted patch
    // text, so key-presence alone is not evidence that a patch exists.
    return !patchText.isEmpty();
}

QString fetchedPatchStatus(const QVariantMap &entry){
    if (entry.value("changes").toInt() == 0) {
        return QStringLiteral("not_provided_no_textual_changes");
    }

    const QString savedStatus = entry.value("patch_status").toString();
    if (!savedStatus.isEmpty()) {
        return savedStatus;
    }

    if (fetchedPatchAvailable(entry)) {
        return QStringLiteral("available");
    }

    return QStringLiteral("missing_from_github");
}

gitFiles gitFilesFromFetchedPrData(const QVariantMap &prData){
    gitFiles output;

    const QVariantList commitsData = prData.value("commits_data").toList();

    for (const QVariant &filesVariant : commitsData) {
        const QVariantMap files = filesVariant.toMap();

        for (QVariantMap::const_iterator it = files.constBegin(); it != files.constEnd(); ++it) {
            const QString fileName = it.key();
            const QVariantList fileEntries = it.value().toList();

            if (fileEntries.isEmpty()) {
                continue;
            }

            const QVariantMap firstEntry = fileEntries.first().toMap();
            const QString status =
                firstEntry.value("status").toString().trimmed().toLower();

            if (status == "added") {
                if (!output.addedFiles.contains(fileName)) {
                    output.addedFiles.append(fileName);
                }
                continue;
            }

            if (status == "renamed") {
                const QString previousFileName =
                    firstEntry.value("previous_filename").toString().trimmed();

                // Preserve every renamed entry for output even if GitHub ever
                // omits previous_filename. The lookup map below can only be
                // built when the old path is known, but the saved JSON should
                // never silently make a renamed file disappear.
                output.renamedFileRecords.append(
                    qMakePair(previousFileName, fileName)
                    );

                if (!previousFileName.isEmpty()) {
                    output.renamedFiles[previousFileName] = fileName;
                } else {
                    qWarning() << "GitHub reported a renamed file without"
                                  "previous_filename metadata:"
                               << fileName;
                }
            }
        }
    }

    return output;
}
