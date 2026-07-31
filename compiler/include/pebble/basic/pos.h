#pragma once
#include <compare>
#include <cstdint>

namespace pebble::basic {

struct Pos {
    std::uint32_t Start;
    std::uint32_t FileId;

    Pos (std::uint32_t start, std::uint32_t fileId) : Start (start), FileId (fileId) {}

    auto
    operator<=> (const Pos &) const = default;
};

}
