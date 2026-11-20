#ifndef TOKEN_STORE_H
#define TOKEN_STORE_H

#include <QString>
#include <QStringList>

// The GitHub tokens the user pastes into the interface are also mirrored to
// tokens.txt in the working directory. A long dataset batch reads that file
// back between runs, so tokens can be replaced without restarting MOVis.

// Path of the file the tokens are mirrored to.
QString tokensFilePath();

// Reads one token per line. Returns `fallback` unchanged if the file cannot be
// read, so a batch keeps running on the tokens it already had.
QStringList loadTokensFile(const QStringList &fallback);

// Writes the token text verbatim. Returns false and fills `errorOut` if the
// file could not be written.
bool saveTokensFile(const QString &text, QString *errorOut = nullptr);

#endif // TOKEN_STORE_H
