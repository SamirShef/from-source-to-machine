#pragma once
#include "pebble/cl/prefix_style.h"
#include <string_view>

namespace pebble::cl {

inline bool
CheckPrefix (std::string_view arg, PrefixStyle style, std::size_t &prefLen) {
    if (arg.starts_with ("--")) {
        prefLen = 2;
        return style == PrefixStyle::OnlyTwo || style == PrefixStyle::OneOrTwo;
    }
    if (arg.starts_with ("-")) {
        prefLen = 1;
        return style == PrefixStyle::OnlyOne || style == PrefixStyle::OneOrTwo;
    }
    prefLen = 0;
    return false;
}

bool
ParseCommandLineOptions (int argc, char **argv);

}
