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

    Token
    tokenizeStrLit ();

    Token
    tokenizeCharLit ();

    Token
    tokenizeOp ();

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
};

}
