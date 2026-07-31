#pragma once
#include "pebble/basic/source_mgr.h"
#include "pebble/diagnostic/annotation.h"
#include "pebble/diagnostic/builder.h"
#include "pebble/diagnostic/colors.h"
#include <algorithm>
#include <cmath>
#include <iostream>

namespace pebble::diagnostic {

inline int
DigitCount (std::uint32_t line) {
    if (line == 0) {
        return 1;
    }
    return static_cast<int> (std::log10 (line)) + 1;
}

class DiagnosticEngine {
    basic::SourceMgr              &_mgr;
    std::vector<DiagnosticBuilder> _builders;
    bool                           _hasErrs{};

public:
    explicit DiagnosticEngine (basic::SourceMgr &mgr) : _mgr (mgr) {}

    DiagnosticBuilder &
    Report (DiagCode code, const std::string &msg, DiagSeverity severity) {
        _builders.emplace_back (code, msg, severity);
        if (severity == DiagSeverity::Error) {
            _hasErrs = true;
        }
        return _builders.back ();
    }

    bool
    HasErrors () const {
        return _hasErrs;
    }

    void
    Render () {
        int i = 0;
        for (DiagnosticBuilder &diag : _builders) {
            if (i != 0) {
                std::cerr << '\n';
            }
            renderDiag (diag);
            ++i;
        }
    }

    std::vector<DiagnosticBuilder> &
    Builders () {
        return _builders;
    }

    void
    SortDiagSpans () {
        for (auto &diag : _builders) {
            sortDiagSpans (diag);
        }
    }

private:
    void
    sortDiagSpans (DiagnosticBuilder &diag);

    void
    renderDiag (DiagnosticBuilder &diag) {
        sortDiagSpans (diag);
        printDiagnosticHeader (diag);
        printDiagnosticBody (diag);
    }

    static void
    printDiagnosticHeader (DiagnosticBuilder &diag);

    void
    printDiagnosticBody (DiagnosticBuilder &diag) {
        auto maxAnnotation = std::max_element (
            diag.Annotations ().begin (),
            diag.Annotations ().end (),
            [&] (const Annotation &a, const Annotation &b) {
                return _mgr.FindLoc (a.Span.Start).Line
                       < _mgr.FindLoc (b.Span.Start).Line;
            });
        auto loc          = _mgr.FindLoc (maxAnnotation->Span.Start);
        auto maxLine      = loc.Line;
        auto maxLineWidth = DigitCount (maxLine);
        std::cerr << color::RESET << std::string (maxLineWidth, ' ') << "--> "
                  << _mgr.GetFile (maxAnnotation->Span.Start.FileId).Name << '\n';
        for (const auto &annotation : diag.Annotations ()) {
            if (annotation == *diag.Annotations ().begin ()) {
                std::cerr << color::RESET;
                std::cerr << std::string (maxLineWidth, ' ') << " |\n";
            }
            printAnnotation (annotation, maxLineWidth);
            std::cerr << std::string (maxLineWidth, ' ') << " |\n";
        }
    }

    void
    printAnnotation (const Annotation &annotation, std::uint32_t maxLineWidth);
};

}
