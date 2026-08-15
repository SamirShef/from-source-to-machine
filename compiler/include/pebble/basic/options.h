#pragma once
#include "pebble/cl/option.h"

namespace pebble {

inline cl::OptBool ShortHelp ("h", "Display available options", cl::PrefixStyle::OnlyOne);

inline cl::OptBool Help ("help", "Display available options", cl::PrefixStyle::OnlyTwo);

inline cl::OptBool DumpTokens ("dump-tokens", "Print lexer tokens to stdout");

inline cl::OptString
    Emit ("emit", "Type of output for the compiler to emit", cl::PrefixStyle::OnlyTwo);

inline cl::OptPositionalList InputFiles ("input files", "Input files");

}
