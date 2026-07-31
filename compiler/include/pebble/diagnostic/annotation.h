#pragma once
#include "pebble/diagnostic/span.h"
#include <string>

namespace pebble::diagnostic {

struct Annotation {
    struct Span Span;
    std::string Label;
    bool        IsPrimary = true;

    Annotation (struct Span span, std::string label, bool isPrimary)
        : Span (span), Label (std::move (label)), IsPrimary (isPrimary) {}

    auto
    operator<=> (const Annotation &) const = default;
};

}
