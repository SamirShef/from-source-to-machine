#include "pebble/lexer/lexer.h"
#include "pebble/basic/pos.h"

namespace pebble {

Token
Lexer::NextToken () {
    return {
        TokenKind::Eof,
        "",
        { basic::Pos{ 0, 0 }, basic::Pos{ 0, 0 } }
    };
}

}
