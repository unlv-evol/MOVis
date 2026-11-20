#ifndef GITHUBHELPERS_H
#define GITHUBHELPERS_H

#include <QString>
#include <QStringList>

struct DivergenceResult {
    QString divergeDate;    // divergence_date
    QString leastDate;      // least_date / cut-off date
    QString mainlineRepo;   // canonical GitHub full_name
    QString variantRepo;    // canonical GitHub full_name
};

// Resolves where `variant` diverged from `mainline`, along with the latest
// date worth classifying and the canonical names GitHub reports for both
// repositories (either may have been renamed or transferred).
DivergenceResult divergence_date_qt(const QString& mainline,
                                    const QString& variant,
                                    const QStringList& tokenList,
                                    int ct,
                                    QString leastDate = QString(),
                                    QString divergeDate = QString());

// Override for detached pairs: repositories that share code but are not in
// the same GitHub fork network (e.g. Red5/red5-server -> ant-media/Ant-Media-Server).
// GitHub's compare API cannot relate those, so no merge base exists. Instead
// this estimates the range from repository metadata: the divergence date is
// the variant's creation date (never earlier than the source's), and the
// cut-off is the older of the two updated_at timestamps, as in the normal
// check. Empty dates mean either repository could not be resolved.
DivergenceResult detached_pair_dates_qt(const QString& mainline,
                                        const QString& variant,
                                        const QStringList& tokenList);

#endif // GITHUBHELPERS_H
