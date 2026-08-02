#pragma once
#include "pebble/lexer/token.h"

namespace pebble {

class Lexer {
public:
    Token
    NextToken ();
};

}
