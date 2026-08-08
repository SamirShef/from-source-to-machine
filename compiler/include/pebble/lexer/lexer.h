#pragma once
#include "pebble/diagnostic/engine.h"
#include "pebble/lexer/token.h"

namespace pebble {

class Lexer {
    diagnostic::DiagnosticEngine &_diag;
    std::uint32_t                 _fileId;
    std::uint32_t                 _pos{};
    const std::string            &_source;

public:
    Lexer (diagnostic::DiagnosticEngine &diag, std::uint32_t fileId)
        : _diag (diag),
          _fileId (fileId),
          _source (diag.SourceMgr ().GetFile (fileId).Content) {}

    Token
    NextToken ();

private:
    Token
    tokenizeIdOrKeyword ();

    Token
    tokenizeNumLit ();

    void
    skipNumSuffix ();

    Token
    tokenizeStrLit ();

    Token
    tokenizeCharLit ();

    Token
    tokenizeOp ();

    char
    parseEscapeSequence ();

    void
    skipComment ();

    void
    skipMultilineComment ();

    void
    skipSingleComment ();

    void
    skipSpaces ();

    char
    advance ();

    char
    peek (int relPos = 0) const;

    constexpr bool
    isAtEnd () {
        return _pos >= _source.size ();
    }

    static constexpr bool
    isHexDigit (char c) noexcept {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    }

    static constexpr bool
    isOctalDigit (char c) noexcept {
        return c >= '0' && c <= '7';
    }

    void
    checkAndConsumeHexDigits (int digitCount);

    void
    checkAndConsumeUnicodeDigits (int digitCount);

    void
    checkAndConsumeOctalDigits (int digitCount);
};

}
