#include "headers/helpers/regexHelpers.h"
#include "headers/helpers/fileHelpers.h"

QString changeHunkName(const QString &selectedAbsPath)
{
    QFileInfo fileInfo(selectedAbsPath);
    const QString fileName = fileInfo.fileName();

    // Match:
    // hunk_12_additions.java  -> hunk_12_full_add.java
    // hunk_12_deletions.java  -> hunk_12_full_del.java
    static const QRegularExpression reAddDel(
        R"(^(hunk_\d+)_(additions|deletions)(\.[^/\\]+)?$)"
        );

    const QRegularExpressionMatch match = reAddDel.match(fileName);
    if (!match.hasMatch())
        return selectedAbsPath;

    const QString prefix = match.captured(1);             // hunk_12
    const QString kind = match.captured(2);             // additions/deletions
    const QString ext = match.captured(3);             // ".java"/etc...

    QString fullSuffix;
    if(kind == "additions"){
        fullSuffix = "full_add";
    }else{
        fullSuffix = "full_del";
    }

    const QString fullName = prefix + "_" + fullSuffix + ext;

    // Creates a full file path
    const QString candidate = QDir(fileInfo.absolutePath()).filePath(fullName);

    if (QFileInfo::exists(candidate))
        return canonicalOrOriginal(candidate);

    // If the full file doesn't exist, fall back to the selected file.
    return selectedAbsPath;
}
