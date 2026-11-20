#include "headers/results/HunkPaths.h"

#include "headers/helpers/fileHelpers.h"
#include <QFileInfo>
#include <QRegularExpression>

static bool reportedPathMatches(const QString &reportedName,
                                const QString &relativeToRepo,
                                const QString &relativeToPr,
                                const QString &fileName){
    const QString reported = normalizePath(reportedName).toLower();
    const QString repoRelative = normalizePath(relativeToRepo).toLower();
    const QString prRelative = normalizePath(relativeToPr).toLower();
    const QString baseName = QFileInfo(normalizePath(fileName)).fileName().toLower();

    if (!repoRelative.isEmpty() &&
        (reported == repoRelative || reported.endsWith("/" + repoRelative))) {
        return true;
    }

    if (!prRelative.isEmpty() &&
        (reported == prRelative || reported.endsWith("/" + prRelative))) {
        return true;
    }

    const QString reportedBase = QFileInfo(reported).fileName();
    return !baseName.isEmpty() && reportedBase == baseName;
}


QString reportedHunkKind(const QString &reportedName){
    const QString baseName = QFileInfo(normalizePath(reportedName)).fileName();

    static const QRegularExpression hunkPattern(
        R"(^hunk_\d+_(additions|deletions|full_add|full_del)(\.[^/\\]+)?$)",
        QRegularExpression::CaseInsensitiveOption
        );

    const QRegularExpressionMatch match = hunkPattern.match(baseName);
    if (!match.hasMatch()) {
        return QString();
    }

    return match.captured(1).toLower();
}

bool reportedPathMatchesRight(const QString &reportedName,
                              const JscpdMatchKeys &keys){
    return reportedPathMatches(reportedName,
                               keys.rightRelRepo,
                               keys.rightRelPR,
                               keys.rightFileName);
}

QString displayedHunkKey(const QString &reportedName){
    const QString baseName = QFileInfo(normalizePath(reportedName)).fileName();

    static const QRegularExpression hunkPattern(
        R"(^(hunk_\d+)_(additions|deletions|full_add|full_del)(\.[^/\\]+)?$)",
        QRegularExpression::CaseInsensitiveOption
        );

    const QRegularExpressionMatch match = hunkPattern.match(baseName);
    if (!match.hasMatch()) {
        return QString();
    }

    const QString prefix = match.captured(1);
    const QString kind = match.captured(2).toLower();
    const QString extension = match.captured(3);

    if (kind == "additions" || kind == "full_add") {
        return (prefix + "_additions" + extension).toCaseFolded();
    }

    return (prefix + "_deletions" + extension).toCaseFolded();
}

bool contextHunkDisplayKeys(const QString &reportedName,
                            QString &additionsKey,
                            QString &deletionsKey){
    const QString baseName = QFileInfo(normalizePath(reportedName)).fileName();

    static const QRegularExpression contextPattern(
        R"(^(hunk_\d+)_context(\.[^/\\]+)?$)",
        QRegularExpression::CaseInsensitiveOption
        );

    const QRegularExpressionMatch match = contextPattern.match(baseName);
    if (!match.hasMatch()) {
        additionsKey.clear();
        deletionsKey.clear();
        return false;
    }

    const QString prefix = match.captured(1);
    const QString extension = match.captured(2);

    additionsKey = (prefix + "_additions" + extension).toCaseFolded();
    deletionsKey = (prefix + "_deletions" + extension).toCaseFolded();
    return true;
}

int displayedHunkLineOffset(const QString &reportedName){
    const QString baseName = QFileInfo(normalizePath(reportedName)).fileName();

    // additions/deletions omit the @@ line that exists in the displayed
    // full_add/full_del file. Reports against a full_* file already include it.
    if (baseName.contains("_full_add", Qt::CaseInsensitive) ||
        baseName.contains("_full_del", Qt::CaseInsensitive)) {
        return 0;
    }

    return 1;
}
