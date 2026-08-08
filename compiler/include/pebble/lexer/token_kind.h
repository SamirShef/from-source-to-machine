#pragma once
#include <cstdint>
#include <cstdlib>

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
    Float64, // type `float64`

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

    BoolLit, // bool literal
    CharLit, // character literal
    NumLit,  // any numeric literal
    StrLit,  // string literal

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

inline const char *
TokenKindToString (TokenKind kind) {
#define variant(kind)                                                                    \
    case TokenKind::kind:                                                                \
        return #kind;

    switch (kind) {
        variant (Id);
        variant (Bool);
        variant (Char);
        variant (Int8);
        variant (Int16);
        variant (Int32);
        variant (Int64);
        variant (Uint8);
        variant (Uint16);
        variant (Uint32);
        variant (Uint64);
        variant (Float32);
        variant (Float64);
        variant (Var);
        variant (Const);
        variant (Fn);
        variant (Ret);
        variant (If);
        variant (Else);
        variant (For);
        variant (Break);
        variant (Continue);
        variant (Struct);
        variant (Nil);
        variant (Import);
        variant (Extern);
        variant (BoolLit);
        variant (CharLit);
        variant (NumLit);
        variant (StrLit);
        variant (Semi);
        variant (Comma);
        variant (Dot);
        variant (LParen);
        variant (RParen);
        variant (LBrace);
        variant (RBrace);
        variant (LBracket);
        variant (RBracket);
        variant (Question);
        variant (Colon);
        variant (Eq);
        variant (Amp);
        variant (Pipe);
        variant (AmpAmp);
        variant (PipePipe);
        variant (Plus);
        variant (Minus);
        variant (Star);
        variant (Slash);
        variant (Percent);
        variant (PlusEq);
        variant (MinusEq);
        variant (StarEq);
        variant (SlashEq);
        variant (PercentEq);
        variant (AndEq);
        variant (OrEq);
        variant (CarretEq);
        variant (Bang);
        variant (BangEq);
        variant (EqEq);
        variant (Lt);
        variant (Gt);
        variant (LtEq);
        variant (GtEq);
        variant (Carret);
        variant (Unknown);
        variant (Eof);
    }

#undef variant

    abort ();
}

}
