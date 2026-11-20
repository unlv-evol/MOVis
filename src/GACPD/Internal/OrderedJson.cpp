#include "headers/GACPD/Internal/OrderedJson.h"

#include <QDebug>
#include <QFileInfo>
#include <QDir>
#include <QJsonArray>
#include <QJsonValue>
#include <QSaveFile>
#include <QStringList>

static QByteArray jsonStringValue(const QString &value){
    QJsonArray wrapper;
    wrapper.append(value);

    QByteArray encoded =
        QJsonDocument(wrapper).toJson(QJsonDocument::Compact);

    if (encoded.size() >= 2 && encoded.front() == '[' && encoded.back() == ']') {
        encoded = encoded.mid(1, encoded.size() - 2);
    }

    return encoded;
}

static QByteArray jsonPrimitiveValue(const QJsonValue &value){
    QJsonArray wrapper;
    wrapper.append(value);

    QByteArray encoded =
        QJsonDocument(wrapper).toJson(QJsonDocument::Compact);

    if (encoded.size() >= 2 && encoded.front() == '[' && encoded.back() == ']') {
        encoded = encoded.mid(1, encoded.size() - 2);
    }

    return encoded;
}

static const QStringList &defaultJsonFieldOrder(){
    // Qt serializes QJsonObject keys lexicographically. These result files are
    // primarily inspected by humans, so keep related fields together instead
    // of presenting every object alphabetically. Unknown/dynamic keys are
    // still preserved after the known fields.
    static const QStringList order = {
        "fileName",
        "fileExtension",
        "language",
        "isConfigurationLanguage",
        "pr",
        "classifiedPR",
        "prTitle",
        "prDescription",
        "prLocation",
        "repoDivergenceDate",
        "cutoffDate",
        "mainline",
        "divergentRepo",
        "changeStatus",
        "renamed",
        "renamedFileName",
        "patchAvailable",
        "patchStatus",
        "addedFilesSkipped",
        "renamedFiles",
        "from",
        "to",
        "divergentPath",
        "divergentPathType",
        "classification",
        "errorReason",
        "similarityChecks",
        "hunks",
        "number",
        "additionsClassification",
        "deletionsClassification",
        "checkName",
        "tokenSize",
        "similarityPercent",
        "directChangeClone",
        "raw",
        "files",
        "recommendations",
        "request",
        "url",
        "page",
        "attempts",
        "attempt",
        "response",
        "commit_list_requested",
        "commit_before",
        "commit_sha_before",
        "github_pr_commit_count",
        "fetched_pr_commits",
        "commits_fetch_complete",
        "commits_fetch_note",
        "commits",
        "commit_requests",
        "commit_graphql_requests",
        "file_requests",
        "github_changed_files",
        "fetched_files",
        "files_fetch_complete",
        "status",
        "source",
        "statistics",
        "formats",
        "duplicates",
        "firstFile",
        "secondFile",
        "name",
        "start",
        "end",
        "startLoc",
        "endLoc",
        "line",
        "column"
    };

    return order;
}

static void appendKnownKeys(const QJsonObject &object,
                            const QStringList &keys,
                            QStringList &ordered,
                            QSet<QString> &used){
    for (const QString &key : keys) {
        if (object.contains(key) && !used.contains(key)) {
            ordered.append(key);
            used.insert(key);
        }
    }
}

static QStringList orderedJsonKeys(const QJsonObject &object,
                                   const QStringList &preferredOrder){
    QStringList ordered;
    QSet<QString> used;

    appendKnownKeys(object, preferredOrder, ordered, used);
    appendKnownKeys(object, defaultJsonFieldOrder(), ordered, used);

    // QJsonObject::keys() is deterministic. Only truly dynamic/unrecognized
    // fields fall back to that ordering; normal result metadata uses the
    // explicit logical order above.
    for (const QString &key : object.keys()) {
        if (!used.contains(key)) {
            ordered.append(key);
        }
    }

    return ordered;
}

static QByteArray serializeOrderedJsonValue(const QJsonValue &value,
                                            int indentLevel,
                                            const QStringList &preferredOrder = {});


static QByteArray serializeOrderedJsonArray(const QJsonArray &array,
                                            int indentLevel){
    if (array.isEmpty()) {
        return "[]";
    }

    const QByteArray indent(indentLevel * 4, ' ');
    const QByteArray childIndent((indentLevel + 1) * 4, ' ');

    QByteArray json = "[\n";
    for (qsizetype i = 0; i < array.size(); ++i) {
        json += childIndent;
        json += serializeOrderedJsonValue(array.at(i), indentLevel + 1);
        if (i + 1 < array.size()) {
            json += ',';
        }
        json += '\n';
    }

    json += indent;
    json += ']';
    return json;
}

static QByteArray serializeOrderedJsonValue(const QJsonValue &value,
                                            int indentLevel,
                                            const QStringList &preferredOrder){
    if (value.isObject()) {
        return serializeOrderedJsonObject(
            value.toObject(),
            indentLevel,
            preferredOrder
            );
    }

    if (value.isArray()) {
        return serializeOrderedJsonArray(value.toArray(), indentLevel);
    }

    return jsonPrimitiveValue(value);
}

QByteArray serializeOrderedJsonObject(const QJsonObject &object,
                                      int indentLevel,
                                      const QStringList &preferredOrder){
    if (object.isEmpty()) {
        return "{}";
    }

    const QByteArray indent(indentLevel * 4, ' ');
    const QByteArray childIndent((indentLevel + 1) * 4, ' ');
    const QStringList keys = orderedJsonKeys(object, preferredOrder);

    QByteArray json = "{\n";

    for (int i = 0; i < keys.size(); ++i) {
        const QString &key = keys[i];
        json += childIndent;
        json += jsonStringValue(key);
        json += ": ";
        json += serializeOrderedJsonValue(object.value(key), indentLevel + 1);

        if (i + 1 < keys.size()) {
            json += ',';
        }
        json += '\n';
    }

    json += indent;
    json += '}';
    return json;
}

bool writeOrderedJsonObjectFile(const QString &path,
                                const QJsonObject &root,
                                const char *description,
                                const QStringList &topLevelOrder){
    QByteArray json =
        serializeOrderedJsonObject(root, 0, topLevelOrder);
    json += '\n';

    QSaveFile file(path);

    // Write the serialized JSON bytes exactly as produced. Do NOT open in
    // QIODevice::Text mode here: on Windows Qt translates every \n byte to
    // \r\n on disk. The verification below intentionally compares exact bytes, so
    // text-mode newline conversion would make every valid JSON write appear
    // corrupt even though the file was saved successfully.
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Unable to open" << description << "for writing:" << path;
        return false;
    }

    if (file.write(json) != json.size()) {
        qWarning() << "Unable to fully write" << description << ":" << path;
        file.cancelWriting();
        return false;
    }

    if (!file.commit()) {
        qWarning() << "Unable to commit" << description << ":" << path;
        return false;
    }

    // Do not treat a successful commit call as sufficient for research data.
    // Re-open the final path and verify the exact bytes that were requested
    // are present. This catches unexpected filesystem/storage failures instead
    // of silently counting a result as saved.
    QFile verifyFile(path);
    if (!verifyFile.open(QIODevice::ReadOnly)) {
        qWarning() << "Unable to reopen" << description
                   << "for verification:" << path;
        return false;
    }

    const QByteArray savedJson = verifyFile.readAll();
    verifyFile.close();

    if (savedJson != json) {
        qWarning() << "Verification failed for" << description << ":" << path
                   << "expected bytes:" << json.size()
                   << "saved bytes:" << savedJson.size();
        return false;
    }

    return true;
}

bool writePrApiResponseLog(const QString &path,
                           const QJsonObject &root){
    return writeOrderedJsonObjectFile(
        path,
        root,
        "PR API response log"
        );
}
