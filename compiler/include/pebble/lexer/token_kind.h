#pragma once
#include <cstdint>

namespace pebble {

enum class TokenKind : std::uint8_t {
    Id, // identifier

    Bool,    // type `bool`
    Char,    // type `char`
    Int8,    // type `int8`
    Int16,   // type `int16`
    Int32,   // type `int32`
    Int64,   // type `int64`
    Uint8,   // type `uint8`
    Uint16,  // type `uint16`
    Uint32,  // type `uint32`
    Uint64,  // type `uint64`
    Float32, // type `float32`
    Fload64, // type `float64`

    Var,      // keyword `var`
    Const,    // keyword `const`
    Fn,       // keyword `fn`
    Ret,      // keyword `return`
    If,       // keyword `if`
    Else,     // keyword `else`
    For,      // keyword `for`
    Break,    // keyword `break`
    Continue, // keyword `continue`
    Struct,   // keyword `struct`
    Nil,      // keyword `nil`
    Import,   // keyword `import`
    Extern,   // keyword `extern`

    BoolLit,    // bool literal
    CharLit,    // character literal
    Int8Lit,    // int8 literal
    Int16Lit,   // int16 literal
    Int32Lit,   // int32 literal
    Int64Lit,   // int64 literal
    Uint8Lit,   // uint8 literal
    Uint16Lit,  // uint16 literal
    Uint32Lit,  // uint32 literal
    Uint64Lit,  // uint64 literal
    Float32Lit, // float32 literal
    Float64Lit, // float64 literal
    IntLit,     // integer literal (unresolved width)
    StrLit,     // string literal

    Semi,      // `;`
    Comma,     // `,`
    Dot,       // `.`
    LParen,    // `(`
    RParen,    // `)`
    LBrace,    // `}`
    RBrace,    // `{`
    LBracket,  // `[`
    RBracket,  // `]`
    Question,  // `?`
    Colon,     // `:`
    Eq,        // `=`
    Amp,       // `&`
    Pipe,      // `|`
    AmpAmp,    // `&&`
    PipePipe,  // `||`
    Plus,      // `+`
    Minus,     // `-`
    Star,      // `*`
    Slash,     // `/`
    Percent,   // `%`
    PlusEq,    // `+=`
    MinusEq,   // `-=`
    StarEq,    // `*=`
    SlashEq,   // `/=`
    PercentEq, // `%=`
    AndEq,     // `&=`
    OrEq,      // `|=`
    CarretEq,  // `^=`
    Bang,      // `!`
    BangEq,    // `!=`
    EqEq,      // `==`
    Lt,        // `<`
    Gt,        // `>`
    LtEq,      // `<=`
    GtEq,      // `>=`
    Carret,    // `^`

    Unknown,
    Eof
};

}
