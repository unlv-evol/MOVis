#ifndef PRSIMILARITYPARSER_H
#define PRSIMILARITYPARSER_H

#include <QString>
#include <QTextStream>
#include <QFile>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QFileInfo>
#include "structures/prFileReport.h"

class prSimilarityParser {
public:
    bool load(const QString &filePath, QString *errorOut = nullptr);

    // Query by file name and hunk name (substring match).
    // fileName can be full path OR just basename like "BrokerServer.scala"
    // hunkName can be full hunk file like "src/hunk_1_additions.scala"
    // or substring like "hunk_1_additions"
    bool query(const QString &fileName,
               const QString &hunkName,
               double *similarityOut,
               QString *classificationOut);

    QStringList availableFiles();
    QString getPRTitle() const { return prTitle; }
    QString getPRLocation() const { return prLocation; }


private:
    QVector<FileReport> reports;

    void parseStream(QTextStream &in);

    const FileReport* findFileReport(const QString &fileName);
    const HunkInfo* findHunk(const FileReport &rep, const QString &hunkName);
    QString prTitle;
    QString prLocation;

};

#endif // PRSIMILARITYPARSER_H
