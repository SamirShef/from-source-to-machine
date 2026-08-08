#include "pebble/lexer/lexer.h"
#include "pebble/basic/pos.h"
#include "pebble/diagnostic/codes.h"
#include "pebble/lexer/keywords.h"
#include <cctype>

namespace pebble {

#define pos(start) pebble::basic::Pos ((start), (_fileId))
#define span(start, end) pebble::diagnostic::Span (pos ((start)), pos ((end)))
#define tok(kind, val, span) pebble::Token ((kind), (val), (span))
#define tok2(kind, val, span) pebble::Token (pebble::TokenKind::kind, (val), (span))

Token
Lexer::NextToken () {
    if (peek () == '\0') {
        return tok2 (Eof, "", span (_pos, _pos));
    }
    if (peek () == '/' && (peek (1) == '/' || peek (1) == '*')) {
        skipComment ();
        return NextToken ();
    }
    if (std::isspace (peek ()) != 0) {
        skipSpaces ();
        return NextToken ();
    }

    if (std::isalpha (peek ()) != 0 || peek () == '_') {
        return tokenizeIdOrKeyword ();
    }
    if (std::isdigit (peek ()) != 0 || peek () == '.' && std::isdigit (peek (1)) != 0) {
        return tokenizeNumLit ();
    }
    if (peek () == '\"') {
        return tokenizeStrLit ();
    }
    if (peek () == '\'') {
        return tokenizeCharLit ();
    }
    return tokenizeOp ();
}

Token
Lexer::tokenizeIdOrKeyword () {
    auto start = _pos;
    while (!isAtEnd () && (std::isalnum (peek ()) != 0 || peek () == '_')) {
        advance ();
    }
    std::string_view val (&_source[start], _pos - start);
    auto             kind = TokenKind::Id;
    if (auto it = KEYWORDS.find (val); it != KEYWORDS.end ()) {
        kind = it->second;
    }
    return tok (kind, val, span (start, _pos));
}

Token
Lexer::tokenizeNumLit () {
    auto start = _pos;
    bool hasDot{};
    while (!isAtEnd () && (std::isdigit (peek ()) != 0 || peek () == '.')) {
        if (peek () == '.') {
            if (!hasDot) {
                hasDot = true;
            } else {
                break;
            }
        }
        advance ();
    }
    skipNumSuffix ();
    return tok (
        TokenKind::NumLit,
        std::string_view (&_source[start], _pos - start),
        span (start, _pos));
}

void
Lexer::skipNumSuffix () {
    while (!isAtEnd () && std::isalnum (peek ()) != 0) {
        advance ();
    }
}

Token
Lexer::tokenizeStrLit () {
    auto start = _pos;
    advance (); // skip "
    while (!isAtEnd () && peek () != '\"') {
        parseEscapeSequence ();
    }
    auto tokSpan = span (start, _pos);
    if (peek () == '\0') { // end of file
        _diag
            .Report (
                diagnostic::DiagCode::EUnclosedStrLit,
                "unclosed string literal",
                diagnostic::DiagSeverity::Error)
            .AddAnnotation (tokSpan);
    } else {
        advance (); // skip "
    }
    return tok2 (StrLit, std::string_view (&_source[start], _pos - start), tokSpan);
}

Token
Lexer::tokenizeCharLit () {
    auto start = _pos;
    advance (); // skip '
    while (!isAtEnd () && peek () != '\'') {
        parseEscapeSequence ();
    }
    auto tokSpan = span (start, _pos);
    if (peek () == '\0') { // end of file
        _diag
            .Report (
                diagnostic::DiagCode::EUnclosedCharLit,
                "unclosed character literal",
                diagnostic::DiagSeverity::Error)
            .AddAnnotation (tokSpan);
    } else {
        advance (); // skip '
    }
    return tok2 (CharLit, std::string_view (&_source[start], _pos - start), tokSpan);
}

// NOLINTBEGIN(readability-function-cognitive-complexity)
Token
Lexer::tokenizeOp () {
    auto start = _pos;
    auto kind  = TokenKind::Unknown;

#define single(ch, tok_kind)                                                             \
    case ch:                                                                             \
        kind = TokenKind::tok_kind;                                                      \
        break;

#define pair(ch, next_ch, tok_match, tok_default)                                        \
    case ch:                                                                             \
        if (peek () == (next_ch)) {                                                      \
            advance ();                                                                  \
            kind = TokenKind::tok_match;                                                 \
        } else {                                                                         \
            kind = TokenKind::tok_default;                                               \
        }                                                                                \
        break;

#define triple(ch, ch1, tok1, ch2, tok2, tok_default)                                    \
    case ch:                                                                             \
        if (peek () == (ch1)) {                                                          \
            advance ();                                                                  \
            kind = TokenKind::tok1;                                                      \
        } else if (peek () == (ch2)) {                                                   \
            advance ();                                                                  \
            kind = TokenKind::tok2;                                                      \
        } else {                                                                         \
            kind = TokenKind::tok_default;                                               \
        }                                                                                \
        break;

    switch (advance ()) {
        single (';', Semi);
        single (',', Comma);
        single ('.', Dot);
        single ('(', LParen);
        single (')', RParen);
        single ('{', LBrace);
        single ('}', RBrace);
        single ('[', LBracket);
        single (']', RBracket);
        single ('?', Question);
        single (':', Colon);

        pair ('=', '=', EqEq, Eq);
        pair ('!', '=', BangEq, Bang);
        pair ('+', '=', PlusEq, Plus);
        pair ('-', '=', MinusEq, Minus);
        pair ('*', '=', StarEq, Star);
        pair ('/', '=', SlashEq, Slash);
        pair ('%', '=', PercentEq, Percent);
        pair ('^', '=', CarretEq, Carret);
        pair ('<', '=', LtEq, Lt);
        pair ('>', '=', GtEq, Gt);

        triple ('&', '=', AndEq, '&', AmpAmp, Amp);
        triple ('|', '=', OrEq, '|', PipePipe, Pipe);

    default:
        break;
    }

#undef triple
#undef pair
#undef single

    return tok (
        kind,
        std::string_view (&_source[start], _pos - start),
        span (start, _pos));
}

// NOLINTEND(readability-function-cognitive-complexity)

char
Lexer::parseEscapeSequence () {
    auto start = _pos;
    char c     = advance ();
    if (c == '\\') {
        switch (advance ()) {
        case 'n':
        case 'r':
        case 't':
        case '\\':
        case '\'':
        case '\"':
        case '0':
        case 'b':
        case 'a':
        case 'f':
        case 'v':
            break;
        case 'x':
            checkAndConsumeHexDigits (2);
            break;
        case 'u':
            checkAndConsumeUnicodeDigits (4);
            break;
        case 'o':
            checkAndConsumeOctalDigits (3);
            break;
        default:
            _diag
                .Report (
                    diagnostic::DiagCode::EInvalidEscapeSequence,
                    std::string ("invalid escape sequence '\\") + peek (-1) + "'",
                    diagnostic::DiagSeverity::Error)
                .AddAnnotation (span (start, _pos), "unknown escape sequence");
            break;
        }
    }
    return c;
}

void
Lexer::skipComment () {
    advance ();
    bool isMultiline = advance () == '*';
    if (isMultiline) {
        skipMultilineComment ();
    } else {
        skipSingleComment ();
    }
}

void
Lexer::skipMultilineComment () {
    while (!isAtEnd () && (peek (-1) != '/' || peek (-2) != '*')) {
        advance ();
    }
}

void
Lexer::skipSingleComment () {
    // clang-format off
    while (!isAtEnd() && advance () != '\n') {}
    // clang-format on
}

void
Lexer::skipSpaces () {
    while (!isAtEnd () && std::isspace (peek ()) != 0) {
        advance ();
    }
}

char
Lexer::advance () {
    auto c = peek ();
    ++_pos;
    return c;
}

char
Lexer::peek (int relPos) const {
    if (_pos + relPos < 0 || _pos + relPos >= _source.size ()) {
        return '\0';
    }
    return _source[_pos + relPos];
}

void
Lexer::checkAndConsumeHexDigits (int digitCount) {
    auto start = _pos - 1;
    int  count = 0;
    while (!isAtEnd () && isHexDigit (peek ())) {
        advance ();
        ++count;
    }

    if (count < digitCount) {
        _diag
            .Report (
                diagnostic::DiagCode::EInvalidEscapeSequence,
                "invalid hex escape",
                diagnostic::DiagSeverity::Error)
            .AddAnnotation (span (start, _pos), "expected 2 hex digits");
    }
}

void
Lexer::checkAndConsumeUnicodeDigits (int digitCount) {
    auto start = _pos - 1;
    int  count = 0;
    while (!isAtEnd () && isHexDigit (peek ())) {
        advance ();
        ++count;
    }

    if (count < digitCount) {
        _diag
            .Report (
                diagnostic::DiagCode::EInvalidEscapeSequence,
                "invalid unicode escape",
                diagnostic::DiagSeverity::Error)
            .AddAnnotation (span (start, _pos), "expected 4 hex digits");
    }
}

void
Lexer::checkAndConsumeOctalDigits (int digitCount) {
    auto start = _pos - 1;
    int  count = 0;
    while (!isAtEnd () && isOctalDigit (peek ())) {
        advance ();
        ++count;
    }

    if (count < digitCount) {
        _diag
            .Report (
                diagnostic::DiagCode::EInvalidEscapeSequence,
                "invalid octal escape",
                diagnostic::DiagSeverity::Error)
            .AddAnnotation (span (start, _pos), "expected 3 octal digits");
    }
}

#undef tok2
#undef tok
#undef span
#undef pos

}
