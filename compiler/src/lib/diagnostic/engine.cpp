#include "pebble/diagnostic/engine.h"
#include "pebble/diagnostic/codes.h"
#include "pebble/diagnostic/colors.h"
#include <format>

namespace pebble::diagnostic {

#define variant(kind, val)                                                               \
    case DiagSeverity::kind:                                                             \
        return val;

static Color
SeverityColor (DiagSeverity severity) {
    switch (severity) {
        variant (Warning, color::YELLOW);
        variant (Error, color::RED);
        variant (Note, color::CYAN);
    }
    return color::RESET;
}

static const char *
SeverityName (DiagSeverity severity) {
    switch (severity) {
        variant (Warning, "warning");
        variant (Error, "error");
        variant (Note, "note");
    }
    return "<unknown>";
}

#undef variant

char
SeverityPrefix (DiagSeverity severity) {
    return (char) toupper (SeverityName (severity)[0]);
}

int
DiagCodeToIntegerCode (DiagCode code) {
    if (code <= ERR_CODE_LAST) {
        return static_cast<int> (code);
    }
    if (code <= WARN_CODE_LAST) {
        return static_cast<int> (
            static_cast<std::uint8_t> (code)
            - static_cast<std::uint8_t> (WARN_CODE_START));
    }
    return 0;
}

void
DiagnosticEngine::sortDiagSpans (DiagnosticBuilder &diag) {
    std::ranges::sort (
        diag.Annotations (),
        [&] (const Annotation &a, const Annotation &b) {
            return _mgr.FindLoc (a.Span.Start).Line < _mgr.FindLoc (b.Span.Start).Line;
        });
}

void
DiagnosticEngine::printDiagnosticHeader (DiagnosticBuilder &diag) {
    std::cerr << color::BOLD << SeverityColor (diag.Severity ());

    std::cerr << SeverityName (diag.Severity ()) << color::RESET << '[';
    std::cerr << color::BOLD << SeverityColor (diag.Severity ());
    std::string errCode = std::format ("{:04}", DiagCodeToIntegerCode (diag.Code ()));
    std::cerr << SeverityPrefix (diag.Severity ()) << errCode << color::RESET
              << "]: " << diag.Msg () << '\n';
}

void
DiagnosticEngine::printAnnotation (
    const Annotation &annotation, std::uint32_t maxLineWidth) {
    auto startLoc = _mgr.FindLoc (annotation.Span.Start);
    auto endLoc   = _mgr.FindLoc (annotation.Span.End);
    std::cerr << color::YELLOW << std::format ("{:{}}", startLoc.Line, maxLineWidth)
              << color::RESET << " | ";
    auto lineContent = _mgr.GetLineContent (annotation.Span.Start.FileId, startLoc.Line);
    std::cerr << lineContent << '\n';
    std::cerr << std::string (maxLineWidth, ' ') << " | ";
    std::cerr << std::string (startLoc.Col - 1, ' ');
    char highlighter = annotation.IsPrimary ? '^' : '-';
    std::cerr << color::RED << std::string (endLoc.Col - startLoc.Col, highlighter);
    std::cerr << color::RESET << ' ' << annotation.Label << '\n';
}

};
