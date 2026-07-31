#pragma once
#include "pebble/basic/pos.h"

namespace pebble::diagnostic {

struct Span {
    basic::Pos Start;
    basic::Pos End;

    Span (basic::Pos start, basic::Pos end) : Start (start), End (end) {}

    auto
    operator<=> (const Span &) const = default;
};

}
