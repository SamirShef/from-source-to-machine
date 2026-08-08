#pragma once
#include "pebble/diagnostic/annotation.h"
#include "pebble/diagnostic/codes.h"
#include "pebble/diagnostic/help.h"
#include "pebble/diagnostic/note.h"
#include "pebble/diagnostic/span.h"
#include <string>
#include <vector>

namespace pebble::diagnostic {

class DiagnosticBuilder {
    DiagCode                _code;
    std::string             _msg;
    DiagSeverity            _severity;
    std::vector<Annotation> _annotations;
    std::vector<Note>       _notes;
    std::vector<Help>       _helps;

public:
    DiagnosticBuilder (DiagCode code, std::string msg, DiagSeverity severity)
        : _code (code), _msg (std::move (msg)), _severity (severity) {}

    DiagnosticBuilder &
    AddAnnotation (Span span, std::string label = "", bool isPrimary = true) {
        _annotations.emplace_back (span, std::move (label), isPrimary);
        return *this;
    }

    DiagnosticBuilder &
    AddAnnotation (
        basic::Pos start, basic::Pos end, std::string label = "", bool isPrimary = true) {
        return AddAnnotation (Span (start, end), std::move (label), isPrimary);
    }

    DiagnosticBuilder &
    AddNote (std::string text) {
        _notes.emplace_back (std::move (text));
        return *this;
    }

    DiagnosticBuilder &
    AddHelp (std::string text) {
        _helps.emplace_back (std::move (text));
        return *this;
    }

    DiagCode
    Code () const {
        return _code;
    }

    const std::string &
    Msg () const {
        return _msg;
    }

    DiagSeverity
    Severity () const {
        return _severity;
    }

    std::vector<Annotation> &
    Annotations () {
        return _annotations;
    }

    const std::vector<Help> &
    Helps () const {
        return _helps;
    }

    const std::vector<Note> &
    Notes () const {
        return _notes;
    }
};

}
