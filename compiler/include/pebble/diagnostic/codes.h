#pragma once
#include <cstdint>
#include <cstdlib>

namespace pebble::diagnostic {

enum class DiagSeverity : std::uint8_t { Warning, Error, Note };

enum class DiagCode : std::uint8_t {
    // errors
    EUnexpectedToken,
    EExpectedExpr,
    EUnclosedStrLit,
    EUnclosedCharLit,
    EIncorrectCharLitLen,
    EIntSuffixForFloat,
    EInvalidNumSuffix,
    EInvalidEscapeSequence,
    EInvalidChar,

    // warnings
    WUnusedVar,
    WLossPrecision,
};

constexpr inline DiagCode ERR_CODE_START = DiagCode::EUnexpectedToken;
constexpr inline DiagCode ERR_CODE_LAST
    = static_cast<DiagCode> (static_cast<std::uint8_t> (DiagCode::WUnusedVar) - 1);

constexpr inline DiagCode WARN_CODE_START = DiagCode::WUnusedVar;
constexpr inline DiagCode WARN_CODE_LAST  = DiagCode::WLossPrecision;

inline const char *
DiagCodeToString (DiagCode code) {
#define variant(kind)                                                                    \
    case DiagCode::kind:                                                                 \
        return #kind;

    switch (code) {
        variant (EUnexpectedToken);
        variant (EExpectedExpr);
        variant (EUnclosedStrLit);
        variant (EUnclosedCharLit);
        variant (EIncorrectCharLitLen);
        variant (EIntSuffixForFloat);
        variant (EInvalidNumSuffix);
        variant (EInvalidEscapeSequence);
        variant (EInvalidChar);
        variant (WUnusedVar);
        variant (WLossPrecision);
    }

#undef variant
    abort ();
}

}
