#include "pebble/lexer/lexer.h"
#include "pebble/basic/pos.h"
#include "pebble/diagnostic/codes.h"
#include "pebble/lexer/keywords.h"
#include <cctype>
#include <cstdlib>

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

    if (std::isalpha (peek ()) != 0) {
        return tokenizeIdOrKeyword ();
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
    while (std::isalnum (peek ()) != 0) {
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
    abort ();
}

Token
Lexer::tokenizeStrLit () {
    auto start = _pos;
    advance (); // skip "
    // clang-format off
    while (peek() != '\0' && advance() != '\"') {}
    // clang-format on
    auto tokSpan = span (start, _pos);
    if (peek () == '\0') { // end of file
        _diag
            .Report (
                diagnostic::DiagCode::EUnclosedStrLit,
                "unclosed string literal",
                diagnostic::DiagSeverity::Error)
            .AddAnnotation (tokSpan);
    }
    return tok2 (StrLit, std::string_view (&_source[start], _pos - start), tokSpan);
}

Token
Lexer::tokenizeCharLit () {
    auto start = _pos;
    advance (); // skip '
    // clang-format off
    while (peek() != '\0' && advance() != '\'') {}
    // clang-format on
    auto tokSpan = span (start, _pos);
    if (peek () == '\0') { // end of file
        _diag
            .Report (
                diagnostic::DiagCode::EUnclosedCharLit,
                "unclosed character literal",
                diagnostic::DiagSeverity::Error)
            .AddAnnotation (tokSpan);
    }
    return tok2 (CharLit, std::string_view (&_source[start], _pos - start), tokSpan);
}

Token
Lexer::tokenizeOp () {
    auto start = _pos;
    advance ();
    return tok2 (Unknown, "", span (start, _pos));
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
    while (peek (-1) != '/' || peek (-2) != '*') {
        advance ();
    }
}

void
Lexer::skipSingleComment () {
    // clang-format off
    while (advance () != '\n') {}
    // clang-format on
}

void
Lexer::skipSpaces () {
    // clang-format off
    while (std::isspace(advance()) == 0) {}
    // clang-format on
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

#undef pos
#undef span
#undef tok

}
