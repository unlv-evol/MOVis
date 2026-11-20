#include "headers/helpers/userInput.h"

#include <QRegularExpression>

QString removeInvisibleCharacters(const QString &text)
{
    QString cleaned;
    cleaned.reserve(text.size());

    for (const QChar ch : text) {
        // Unicode "format" characters (category Cf) are invisible: zero-width
        // space/joiners (U+200B-U+200D), word joiner (U+2060), BOM (U+FEFF),
        // soft hyphen (U+00AD) and bidi marks. Web pages insert them after "/"
        // as line-break hints, and they survive copy/paste.
        if (ch.category() == QChar::Other_Format) {
            continue;
        }
        cleaned.append(ch);
    }

    return cleaned;
}

QString normalizeGithubToken(const QString &rawToken)
{
    QString token = removeInvisibleCharacters(rawToken).trimmed();

    if (token.startsWith("Bearer ", Qt::CaseInsensitive)) {
        token = token.mid(7).trimmed();
    } else if (token.startsWith("token ", Qt::CaseInsensitive)) {
        token = token.mid(6).trimmed();
    }

    token.remove(' ');
    return token;
}

QStringList parseGithubTokenLines(const QString &text)
{
    QStringList tokens;

    for (const QString &line : text.split('\n', Qt::SkipEmptyParts)) {
        const QString token = normalizeGithubToken(line);
        if (!token.isEmpty()) {
            tokens.append(token);
        }
    }

    return tokens;
}

QStringList splitOnWhitespace(const QString &text)
{
    // Unicode-aware so non-breaking and other exotic spaces also separate items.
    static const QRegularExpression whitespace(
        "\\s+", QRegularExpression::UseUnicodePropertiesOption);

    QStringList result;

    const QString cleaned = removeInvisibleCharacters(text);

    for (const QString &item : cleaned.split(whitespace, Qt::SkipEmptyParts)) {
        if (item == "-") {
            continue;
        }

        result.append(item);
    }

    return result;
}
