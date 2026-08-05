#pragma once
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
};

}
