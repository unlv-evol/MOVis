#ifndef HELPERS_GACPD_H
#define HELPERS_GACPD_H

#include "../Structures/RepoDatesResults.h"
#include "../Structures/RepoDateInfo.h"
#include "headers/GACPD/Methods/common.h"
#include <QDir>
#include <QProcess>

RepoDatesResults repoDates(const QString& repo,
                          const QStringList& tokenList,
                          int& ct);

RepoDateInfo getDivergenceDate(const QString& mainline,
                            const QString& variant,
                            const QStringList& tokenList,
                            int& ct,
                            QString leastDate,
                            QString divergeDate);

// Date estimate for detached pairs: repositories that share code but sit in
// different GitHub fork networks, so the compare API has no merge base.
// divergenceDate = variant created_at (never earlier than the mainline's),
// cutOffDate = the older updated_at. Both empty if either lookup failed.
RepoDateInfo getDetachedPairDates(const QString& mainline,
                                  const QString& variant,
                                  const QStringList& tokenList,
                                  int& ct);

QStringList unique(const QStringList& ogList);

QString nodeExecutable(const QString& projRoot);
bool setupJscpd(const QString& projRoot);

QString savePatch(const QString &storageDir,
                  const QString &fileName,
                  const QString &fileContent,
                  int& dupCount);

bool copyDirectory(const QString& srcDirPath,
                   const QString& destDirPath);

QString file_Name(const QString &name);
QStringList file_Dir(const QString &name);

#endif // HELPERS_GACPD_H
