#pragma once
#include "pebble/basic/pos.h"
#include "pebble/diagnostic/span.h"
#include "pebble/lexer/token_kind.h"
#include <string_view>

namespace pebble {

struct Token {
    TokenKind        Kind;
    std::string_view Val;
    diagnostic::Span Span;

    Token (TokenKind kind, std::string_view val, diagnostic::Span span)
        : Kind (kind), Val (val), Span (span) {}

    Token ()
        : Kind (TokenKind::Unknown),
          Val (std::string_view ("")),
          Span (basic::Pos{ 0, 0 }, basic::Pos{ 0, 0 }) {}
};

}
