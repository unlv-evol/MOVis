// GACPD stage 3: turning the patch text of a pull request into the files the
// clone detector works on.
//
// Each hunk in a patch becomes up to two generated files, hunk_<n>_additions
// and hunk_<n>_deletions, plus a wider hunk_<n>_context file used by the
// separate context scan.

#include "headers/GACPD/GACPD.h"

#include "headers/GACPD/Internal/FetchedPrData.h"
#include "headers/GACPD/Internal/FileLanguages.h"
#include "headers/GACPD/Internal/GitHubRequests.h"
#include "headers/GACPD/Internal/JscpdEvidence.h"
#include "headers/GACPD/Internal/OrderedJson.h"
#include "headers/GACPD/Internal/ResultRecords.h"
#include "headers/GACPD/Methods/helpers_GACPD.h"
#include "headers/GACPD/Methods/common.h"
#include "headers/GACPD/Methods/patchExtractor.h"
#include "headers/GACPD/Methods/appPath.h"

#include <QCryptographicHash>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QTemporaryDir>
#include <exception>
#include <stdexcept>

void GACPD::createDataFrame(){
    dfFilesClasses.clear();
    dfPatchClasses.clear();

    for (QMap<QString, QVariantMap>::const_iterator prIt = repoResults.constBegin(); prIt != repoResults.constEnd(); prIt++) {
        const QString pr = prIt.key();

        const QVariantMap filesMap = prIt.value();

        for (QVariantMap::const_iterator fileIt = filesMap.constBegin(); fileIt != filesMap.constEnd(); fileIt++) {
            const QString filename = fileIt.key();

            try {
                const QVariantMap fileData = fileIt.value().toMap();
                const QVariantMap resultMap = fileData.value("result").toMap();

                const QString patchClass = resultMap.value("patchClass").toString();

                if (isInterestingClass(patchClass)) {
                    FileClassRow row;
                    row.mainline = sourceRepo;
                    row.fork = variantRepo;
                    row.prNumber = pr;
                    row.filename = filename;
                    row.operation = resultMap.value("type").toString();
                    row.fileClassification = patchClass;
                    row.interesting = 1;

                    dfFilesClasses.append(row);
                } else {
                    FileClassRow row;
                    row.mainline = sourceRepo;
                    row.fork = variantRepo;
                    row.prNumber = pr;
                    row.filename = filename;
                    row.operation = "None";
                    row.fileClassification = patchClass;
                    row.interesting = 0;

                    dfFilesClasses.append(row);
                }
            }
            catch (...) {
                // Same behavior as Python:
                // except Exception as e:
                //     pass
            }
        }

        if (prClassifications.contains(pr)) {
            const QVariantMap prMap = prClassifications.value(pr);
            const QString prClass = prMap.value("class").toString();

            PatchClassRow row;
            row.mainline = sourceRepo;
            row.fork = variantRepo;
            row.prNumber = pr;
            row.patchClassification = prClass;
            row.interesting = isInterestingClass(prClass) ? 1 : 0;

            dfPatchClasses.append(row);
        }
    }

    std::sort(dfFilesClasses.begin(), dfFilesClasses.end(),
              [](const FileClassRow &a, const FileClassRow &b) {
                  if (a.prNumber != b.prNumber) {
                      return a.prNumber > b.prNumber;
                  }

                  return a.interesting > b.interesting;
              });

    std::sort(dfPatchClasses.begin(), dfPatchClasses.end(),
              [](const PatchClassRow &a, const PatchClassRow &b) {
                  return a.interesting > b.interesting;
              });
}

void GACPD::saveHunkFiles(int hunkId,
                              const QString &outputDir,
                              const QStringList &additionsHunks,
                              int counts,
                              const QString &typeOfChange){
    QDir dir;

    // Equivalent to: os.makedirs(output_dir, exist_ok=True)
    if (!dir.mkpath(outputDir)) {
        qWarning() << "Could not create output directory:" << outputDir;
        return;
    }

    // Write the hunk file only if applicable
    if (counts != 0) {
        const QString additionsFilePath =
            QDir(outputDir).filePath(
                QString("hunk_%1_%2").arg(hunkId).arg(typeOfChange)
                );

        QFile addFile(additionsFilePath);

        if (!addFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            qWarning() << "Could not write hunk file:" << additionsFilePath;
            return;
        }

        QTextStream out(&addFile);
        out.setEncoding(QStringConverter::Utf8);

        for (const QString &line : additionsHunks) {
            out << line;
        }

        addFile.close();

        // qDebug() << "Saved additions file for hunk" << hunkId << ":" << additionsFilePath;
    }
}

void GACPD::parsePatchFile(const QString& patchFile,
                           const QString& outputDir,
                           const QString& extension){
    QFile patch(patchFile);

    if (!patch.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Could not open patch file:" << patchFile;
        return;
    }

    QTextStream in(&patch);
    in.setEncoding(QStringConverter::Utf8);

    int hunkCount = 0;
    int addCount = 0;
    int delCount = 0;
    int lineCount = 0;
    int fullCount = 0;

    QStringList additionsFile;
    QStringList deletionsFile;
    QStringList contextFile;
    QStringList fullAddHunk;
    QStringList fullDelHunk;

    while (!in.atEnd()) {
        QString line = in.readLine();

        // Preserve Python-like line endings because readLine() removes '\n'
        line += "\n";

        // Detect the start of a new hunk
        if (line.startsWith("@@")) {
            if (hunkCount > 0) {
                saveHunkFiles(hunkCount, outputDir, deletionsFile, delCount,
                              QString("deletions.%1").arg(extension));
                saveHunkFiles(hunkCount, outputDir, additionsFile, addCount,
                              QString("additions.%1").arg(extension));
                saveHunkFiles(hunkCount, outputDir, contextFile, lineCount,
                              QString("context.%1").arg(extension));
                saveHunkFiles(hunkCount, outputDir, fullAddHunk, addCount,
                              QString("full_add.%1").arg(extension));
                saveHunkFiles(hunkCount, outputDir, fullDelHunk, delCount,
                              QString("full_del.%1").arg(extension));
            }

            hunkCount++;

            additionsFile.clear();
            deletionsFile.clear();
            contextFile.clear();
            fullAddHunk.clear();
            fullDelHunk.clear();

            // Counts are per hunk. Keeping them cumulative causes later
            // hunks to be written/classified as if they contained changes
            // that only occurred in earlier hunks.
            addCount = 0;
            delCount = 0;
            lineCount = 0;
            fullCount = 0;

            fullAddHunk.append(line);
            fullDelHunk.append(line);
        }

        if (line.startsWith("+") && !line.startsWith("+++")) {
            additionsFile.append(line.mid(1));
            fullAddHunk.append(line);
            addCount++;
        }
        else if (line.startsWith("-") && !line.startsWith("---")) {
            deletionsFile.append(line.mid(1));
            fullDelHunk.append(line);
            delCount++;
        }
        else if (!line.startsWith("---") &&
                 !line.startsWith("+++") &&
                 !line.startsWith("@@")) {
            additionsFile.append(line);
            deletionsFile.append(line);
            contextFile.append(line);
            fullAddHunk.append(line);
            fullDelHunk.append(line);
            lineCount++;
        }

        fullCount++;
    }

    // Process the last hunk if any
    if (!deletionsFile.isEmpty() || !additionsFile.isEmpty()) {
        saveHunkFiles(hunkCount, outputDir, deletionsFile, delCount,
                      QString("deletions.%1").arg(extension));
        saveHunkFiles(hunkCount, outputDir, additionsFile, addCount,
                      QString("additions.%1").arg(extension));
        saveHunkFiles(hunkCount, outputDir, contextFile, lineCount,
                      QString("context.%1").arg(extension));
        saveHunkFiles(hunkCount, outputDir, fullAddHunk, addCount+fullCount,
                      QString("full_add.%1").arg(extension));
        saveHunkFiles(hunkCount, outputDir, fullDelHunk, delCount+fullCount,
                      QString("full_del.%1").arg(extension));
    }

    patch.close();
}
