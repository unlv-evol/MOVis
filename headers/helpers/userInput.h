#ifndef USER_INPUT_H
#define USER_INPUT_H

#include <QString>
#include <QStringList>

// Parsing for the free-form text the user types or pastes into the interface.
// Everything here is forgiving on purpose: people paste from GitHub docs, from
// spreadsheets and from Markdown bullet lists.

// Drops invisible Unicode format characters (zero-width spaces, BOMs, soft
// hyphens, bidi marks) that web pages embed and copy/paste carries along.
QString removeInvisibleCharacters(const QString &text);

// Turns one pasted line into a bare personal access token. GitHub's own
// examples show tokens as "Bearer <token>" or "token <token>", so accept
// either form and drop any stray whitespace inside the value.
QString normalizeGithubToken(const QString &rawToken);

// One token per line, normalized, with blank lines dropped.
QStringList parseGithubTokenLines(const QString &text);

// Splits on any whitespace and drops standalone "-" bullet markers, so a
// pasted Markdown list works as well as one entry per line. Hyphens inside a
// repository name are preserved.
QStringList splitOnWhitespace(const QString &text);

#endif // USER_INPUT_H
