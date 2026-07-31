#pragma once
#include <cstdint>

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

    // warnings
    WUnusedVar,
    WLossPrecision,
};

constexpr inline DiagCode ERR_CODE_START = DiagCode::EUnexpectedToken;
constexpr inline DiagCode ERR_CODE_LAST
    = static_cast<DiagCode> (static_cast<std::uint8_t> (DiagCode::WUnusedVar) - 1);

constexpr inline DiagCode WARN_CODE_START = DiagCode::WUnusedVar;
constexpr inline DiagCode WARN_CODE_LAST  = DiagCode::WLossPrecision;

}
