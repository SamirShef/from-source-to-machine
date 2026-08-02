#pragma once
#include "pebble/lexer/token_kind.h"
#include <string_view>
#include <unordered_map>

namespace pebble {

#define keyword(val, kind) { (val), pebble::TokenKind::kind }

static inline const std::unordered_map<std::string_view, TokenKind> KEYWORDS{
    keyword ("false", BoolLit),
    keyword ("true", BoolLit),
    keyword ("var", Var),
    keyword ("const", Const),
    keyword ("bool", Bool),
    keyword ("char", Char),
    keyword ("int8", Int8),
    keyword ("uint8", Uint8),
    keyword ("int16", Int16),
    keyword ("uint16", Uint16),
    keyword ("int32", Int32),
    keyword ("uint32", Uint32),
    keyword ("int64", Int64),
    keyword ("uint64", Uint64),
    keyword ("float32", Float32),
    keyword ("float64", Float64),
    keyword ("fn", Fn),
    keyword ("return", Ret),
    keyword ("if", If),
    keyword ("else", Else),
    keyword ("for", For),
    keyword ("break", Break),
    keyword ("continue", Continue),
    keyword ("struct", Struct),
    keyword ("nil", Nil),
    keyword ("import", Import),
    keyword ("extern", Extern)
};

#undef keyword

}
