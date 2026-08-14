#pragma once
#include "pebble/cl/option.h"

namespace pebble {

inline static cl::OptBool DumpTokens ("dump-tokens", "Print lexer tokens to stdout");

inline static cl::OptPositionalList InputFiles ("input files", "Input files");

inline static cl::OptString Test ("foo", "Test option");

}
