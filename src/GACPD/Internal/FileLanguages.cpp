#include "headers/GACPD/Internal/FileLanguages.h"

#include "headers/GACPD/Methods/common.h"
#include <QDir>
#include <QFileInfo>

static bool isConfigurationLanguageFromInternalList(const FileExtensionInfo &extInfo){
    // Keep this metadata tied to the exact internal list that selects the
    // special scripting/configuration-language token ladder (including 10).
    return scriptingLanguages.contains(extInfo.extension);
}

static FileExtensionInfo buildInfo(const QString &lookupKey,
                                   const QStringList &languages,
                                   bool registered){
    FileExtensionInfo info;
    info.isSupported = !languages.isEmpty();
    info.extension = lookupKey;
    info.language = languages.value(0);
    info.registered = registered;
    return info;
}

FileExtensionInfo fileExtensionInfoFromInternalLists(const QString &file){
    const QString normalizedPath = QDir::fromNativeSeparators(file);
    const QString fileName = QFileInfo(normalizedPath).fileName();

    // Exact filenames must be checked before suffixes (Makefile, Dockerfile,
    // go.mod, etc.). These are the same internal mappings used by GACPD.
    QStringList languages = supportedFileExtensions.value(fileName);
    if (!languages.isEmpty()) {
        return buildInfo(fileName, languages, false);
    }

    languages = registeredFileExtensions.value(fileName);
    if (!languages.isEmpty()) {
        return buildInfo(fileName, languages, true);
    }

    const qsizetype lastDot = fileName.lastIndexOf('.');
    QString lookupKey;
    if (lastDot > 0 && lastDot < fileName.size() - 1) {
        lookupKey = fileName.mid(lastDot);
    } else {
        lookupKey = fileName;
    }

    languages = supportedFileExtensions.value(lookupKey);
    if (!languages.isEmpty()) {
        return buildInfo(lookupKey, languages, false);
    }

    languages = registeredFileExtensions.value(lookupKey);
    if (!languages.isEmpty()) {
        return buildInfo(lookupKey, languages, true);
    }

    FileExtensionInfo info;
    // Preserve the actual suffix/name even when no language mapping exists.
    info.extension = lookupKey.isEmpty() ? QStringLiteral("unknown") : lookupKey;
    return info;
}

void addFileLanguageMetadata(QJsonObject &object, const QString &file){
    const FileExtensionInfo extInfo = fileExtensionInfoFromInternalLists(file);
    object["fileExtension"] = extInfo.extension;
    object["language"] = extInfo.language;
    object["isConfigurationLanguage"] =
        isConfigurationLanguageFromInternalList(extInfo);
}

QString jscpdExtensionFor(const FileExtensionInfo &extInfo){
    if (extInfo.extension.startsWith('.')) {
        return extInfo.extension.mid(1);
    }

    const qsizetype lastDot = extInfo.extension.lastIndexOf('.');

    if (lastDot >= 0 && lastDot < extInfo.extension.size() - 1) {
        return extInfo.extension.mid(lastDot + 1);
    }

    return extInfo.extension;
}

bool isExactFileNameMapping(const FileExtensionInfo &extInfo){
    return !extInfo.extension.startsWith('.');
}

QString registeredFormatFor(const FileExtensionInfo &extInfo){
    if (!extInfo.registered) {
        return QString();
    }

    return registeredFileExtensions.value(extInfo.extension).value(0);
}
