#ifndef ORDERED_JSON_H
#define ORDERED_JSON_H

#include <QByteArray>
#include <QJsonObject>
#include <QStringList>
#include <QString>

// Writing JSON with a stable key order.
//
// Result files are read by people and diffed between runs, so they are
// serialized with a fixed field order instead of QJsonDocument's alphabetical
// one. Keys the order does not mention keep their original position after the
// ones it does.

// Writes `object` to `path` atomically. Returns false if it could not be
// written.

// Serializes one object with the given key order applied to its top level.
// `indent` is the starting indentation level.
QByteArray serializeOrderedJsonObject(const QJsonObject &object,
                                      int indent,
                                      const QStringList &topLevelOrder = {});

// Writes `root` to `path` atomically. `description` names the file in the
// warning logged on failure; `topLevelOrder` overrides the default field
// order for the outermost object.
bool writeOrderedJsonObjectFile(const QString &path,
                                const QJsonObject &root,
                                const char *description,
                                const QStringList &topLevelOrder = {});

// The same, for the captured GitHub API responses.
bool writePrApiResponseLog(const QString &path,
                           const QJsonObject &root);

#endif // ORDERED_JSON_H
