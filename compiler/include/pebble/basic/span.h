#pragma once
#include <cstdint>

namespace pebble {

struct Span {
    std::uint32_t Line, Col;
    std::size_t   Offset;

    Span (std::uint32_t line, std::uint32_t col, std::size_t offset)
        : Line (line), Col (col), Offset (offset) {}
};

}
