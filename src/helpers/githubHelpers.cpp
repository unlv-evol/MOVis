#include "headers/helpers/githubHelpers.h"
#include "headers/GACPD/Methods/helpers_GACPD.h"

#include <QDateTime>
#include <QDebug>

DivergenceResult divergence_date_qt(const QString& mainline,
                                    const QString& variant,
                                    const QStringList& tokenList,
                                    int ct,
                                    QString leastDate,
                                    QString divergeDate){
    // Keep the UI calculation identical to GACPD instead of maintaining a
    // second divergence-date implementation. The UI historically passed the
    // token counter by value, so preserve that behavior with a local counter.
    int tokenIndex = ct;
    const RepoDateInfo result = getDivergenceDate(mainline,
                                                   variant,
                                                   tokenList,
                                                   tokenIndex,
                                                   leastDate,
                                                   divergeDate);

    return { result.divergenceDate,
             result.cutOffDate,
             result.mainlineRepo,
             result.variantRepo };
}

DivergenceResult detached_pair_dates_qt(const QString& mainline,
                                        const QString& variant,
                                        const QStringList& tokenList){
    int tokenIndex = 0;
    const RepoDateInfo result =
        getDetachedPairDates(mainline, variant, tokenList, tokenIndex);

    return { result.divergenceDate,
             result.cutOffDate,
             result.mainlineRepo,
             result.variantRepo };
}

