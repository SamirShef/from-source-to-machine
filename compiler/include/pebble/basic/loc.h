#pragma once
#include <cstdint>

namespace pebble::basic {

struct Loc {
    std::uint32_t Line;
    std::uint32_t Col;

    Loc (std::uint32_t line, std::uint32_t col) : Line (line), Col (col) {}
};

}
