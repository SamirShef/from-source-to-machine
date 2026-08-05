#include "pebble/basic/loc.h"
#include "pebble/basic/source_mgr.h"
#include "pebble/diagnostic/engine.h"
#include "pebble/lexer/lexer.h"
#include "pebble/lexer/token_kind.h"
#include <cstdint>
#include <format>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

using namespace pebble;

enum class TestMode : std::uint8_t { Positive, Negative };

struct ExpectedToken {
    std::uint32_t TargetLine;
    std::string   KindStr;
    std::string   Value;
    bool          HasValueCheck{};
    std::uint32_t DirectiveLine;
    bool          Matched{};
};

struct ExpectedError {
    std::uint32_t TargetLine;
    std::string   KindStr;
    std::string   Message;
    bool          HasMessageCheck{};
    std::uint32_t DirectiveLine;
    bool          Matched{};
};

struct TestDirectives {
    TestMode                   Mode = TestMode::Positive;
    std::vector<ExpectedToken> ExpectedTokens;
    std::vector<ExpectedError> ExpectedErrors;
};

struct ActualDiag {
    std::uint32_t Line;
    std::string   KindStr;
    std::string   Message;
    bool          Matched{};
};

static TestDirectives
ParseDirectives (const std::string &source) {
    TestDirectives     directives;
    std::istringstream stream (source);
    std::string        line;
    std::uint32_t      currentLine = 1;

    static const std::regex MODE_REGEX (R"((?://|/\*)\s*mode\s*:\s*(positive|negative))");

    static const std::regex TOKEN_REGEX (
        "(?://|/\\*)\\s*expect(?:ed)?-token(?:@([+-]?\\d+))?\\s*:\\s*([A-Za-z0-9_]+)(?:"
        "\\s+\"([^\"]*)\")?");

    static const std::regex ERROR_REGEX (
        "(?://|/\\*)\\s*expect(?:ed)?-error(?:@([+-]?\\d+))?\\s*:\\s*([A-Za-z0-9_]+)(?:"
        "\\s+\"([^\"]*)\"|\\s+(.+?))?\\s*(?:\\*/)?$");

    while (std::getline (stream, line)) {
        std::smatch match;

        if (std::regex_search (line, match, MODE_REGEX)) {
            std::string modeStr = match[1].str ();
            if (modeStr == "positive") {
                directives.Mode = TestMode::Positive;
            } else if (modeStr == "negative") {
                directives.Mode = TestMode::Negative;
            }
        }

        std::string::const_iterator searchStart (line.cbegin ());
        while (std::regex_search (searchStart, line.cend (), match, TOKEN_REGEX)) {
            ExpectedToken exp{};
            exp.DirectiveLine = currentLine;
            exp.TargetLine    = currentLine;

            if (match[1].matched) {
                int offset     = std::stoi (match[1].str ());
                exp.TargetLine = static_cast<std::uint32_t> (
                    static_cast<int> (currentLine) + offset);
            }

            exp.KindStr = match[2].str ();
            if (match[3].matched) {
                exp.Value         = match[3].str ();
                exp.HasValueCheck = true;
            }

            directives.ExpectedTokens.push_back (exp);
            searchStart = match[0].second;
        }

        searchStart = line.cbegin ();
        while (std::regex_search (searchStart, line.cend (), match, ERROR_REGEX)) {
            ExpectedError exp{};
            exp.DirectiveLine = currentLine;
            exp.TargetLine    = currentLine;

            if (match[1].matched) {
                int offset     = std::stoi (match[1].str ());
                exp.TargetLine = static_cast<std::uint32_t> (
                    static_cast<int> (currentLine) + offset);
            }

            exp.KindStr = match[2].str ();
            if (match[3].matched) {
                exp.Message         = match[3].str ();
                exp.HasMessageCheck = true;
            } else if (match[4].matched && !match[4].str ().empty ()) {
                exp.Message         = match[4].str ();
                exp.HasMessageCheck = true;
            }

            directives.ExpectedErrors.push_back (exp);
            searchStart = match[0].second;
        }

        ++currentLine;
    }

    return directives;
}

static bool
RunCompilerDriver (
    const std::string            &path,
    basic::SourceMgr             &mgr,
    diagnostic::DiagnosticEngine &diag,
    std::vector<Token>           &outTokens) {
    std::ifstream file (path);
    if (!file.is_open ()) {
        std::cerr << std::format ("TestRunner Error: Cannot open file '{}'\n", path);
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf ();
    auto fileId = mgr.AddFile (path, buffer.str ());

    Lexer lexer (diag, fileId);
    while (true) {
        Token tok = lexer.NextToken ();
        if (tok.Kind == TokenKind::Eof) {
            break;
        }
        outTokens.push_back (tok);
    }

    return true;
}

static bool
VerifyTokens (
    const std::vector<ExpectedToken> &expected,
    const std::vector<Token>         &actual,
    basic::SourceMgr                 &mgr) {
    bool hasErrors = false;

    std::vector<bool> actualMatched (actual.size (), false);
    auto              expectedList = expected;

    for (auto &exp : expectedList) {
        bool found = false;

        for (size_t i = 0; i < actual.size (); ++i) {
            if (actualMatched[i]) {
                continue;
            }

            const auto &act = actual[i];
            auto        loc = mgr.FindLoc (act.Span.Start);

            if (loc.Line != exp.TargetLine) {
                continue;
            }

            std::string actualKindStr (TokenKindToString (act.Kind));
            if (actualKindStr != exp.KindStr) {
                continue;
            }

            if (exp.HasValueCheck && act.Val != exp.Value) {
                continue;
            }

            exp.Matched      = true;
            actualMatched[i] = true;
            found            = true;
            break;
        }

        if (!found) {
            hasErrors = true;
            std::cerr << std::format (
                "[FAIL] Directive at line {}: Expected token '{}'",
                exp.DirectiveLine,
                exp.KindStr);
            if (exp.HasValueCheck) {
                std::cerr << std::format (" with value \"{}\"", exp.Value);
            }
            std::cerr << std::format (" on line {} was not found.\n", exp.TargetLine);
        }
    }

    return !hasErrors;
}

static std::vector<ActualDiag>
GetActualErrors (diagnostic::DiagnosticEngine &diag, basic::SourceMgr &mgr) {
    std::vector<ActualDiag> actualErrors;
    for (auto &builder : diag.Builders ()) {
        if (builder.Severity () != diagnostic::DiagSeverity::Error) {
            continue;
        }

        std::uint32_t line = 0;
        if (!builder.Annotations ().empty ()) {
            auto startPos = builder.Annotations ()[0].Span.Start;
            line          = mgr.FindLoc (startPos).Line;
        }

        actualErrors.emplace_back (
            line,
            diagnostic::DiagCodeToString (builder.Code ()),
            builder.Msg (),
            false);
    }
    return actualErrors;
}

static bool
VerifyPositiveDiagnostic (
    std::vector<ActualDiag> &actualErrors, const TestDirectives &directives) {
    if (!actualErrors.empty ()) {
        std::cerr << std::format (
            "[FAIL] Test mode is POSITIVE, but compiler produced {} error(s):\n",
            actualErrors.size ());
        for (const auto &err : actualErrors) {
            std::cerr << std::format (
                "  - Line {}: [{}] {}\n",
                err.Line,
                err.KindStr,
                err.Message);
        }
        return true;
    }
    if (!directives.ExpectedErrors.empty ()) {
        std::cerr << "[FAIL] Test mode is POSITIVE, but contains 'expect-error' "
                     "directives.\n";
        return true;
    }
    return false;
}

static bool
VerifyNegativeDiagnostic (
    std::vector<ActualDiag> &actualErrors, const TestDirectives &directives) {
    if (actualErrors.empty ()) {
        std::cerr << "[FAIL] Test mode is NEGATIVE, but compiler produced NO errors.\n";
        return true;
    }

    auto expectedErrors = directives.ExpectedErrors;
    for (auto &exp : expectedErrors) {
        bool found = false;

        for (auto &act : actualErrors) {
            if (act.Matched) {
                continue;
            }

            if (act.Line == exp.TargetLine && act.KindStr == exp.KindStr) {
                if (exp.HasMessageCheck
                    && act.Message.find (exp.Message) == std::string::npos) {
                    continue;
                }

                exp.Matched = true;
                act.Matched = true;
                found       = true;
                break;
            }
        }

        if (!found) {
            std::cerr << std::format (
                "[FAIL] Directive at line {}: Expected error [{}]",
                exp.DirectiveLine,
                exp.KindStr);
            if (exp.HasMessageCheck) {
                std::cerr << std::format (" \"{}\"", exp.Message);
            }
            std::cerr << std::format (" on line {} was not reported.\n", exp.TargetLine);
            return true;
        }
    }

    for (const auto &act : actualErrors) {
        if (!act.Matched) {
            std::cerr << std::format (
                "[FAIL] Unexpected error reported at line {}: [{}] {}\n",
                act.Line,
                act.KindStr,
                act.Message);
            return true;
        }
    }
    return false;
}

static bool
VerifyDiagnostics (
    const TestDirectives         &directives,
    diagnostic::DiagnosticEngine &diag,
    basic::SourceMgr             &mgr) {

    auto actualErrors = GetActualErrors (diag, mgr);

    bool hasFailed = directives.Mode == TestMode::Positive
                         ? VerifyPositiveDiagnostic (actualErrors, directives)
                         : VerifyNegativeDiagnostic (actualErrors, directives);

    return !hasFailed;
}

int
main (int argc, char **argv) {
    if (argc < 2) {
        std::cerr << std::format ("Usage: {} <path-to-test-file>\n", argv[0]);
        return 1;
    }

    std::string   testFilePath = argv[1];
    std::ifstream file (testFilePath);
    if (!file.is_open ()) {
        std::cerr << std::format ("Error opening test file: {}\n", testFilePath);
        return 1;
    }

    std::string sourceContent (
        (std::istreambuf_iterator<char> (file)),
        std::istreambuf_iterator<char> ());
    file.close ();

    auto directives = ParseDirectives (sourceContent);

    basic::SourceMgr             mgr;
    diagnostic::DiagnosticEngine diag (mgr);
    std::vector<Token>           actualTokens;

    if (!RunCompilerDriver (testFilePath, mgr, diag, actualTokens)) {
        return 1;
    }

    bool tokensOk = VerifyTokens (directives.ExpectedTokens, actualTokens, mgr);
    bool diagsOk  = VerifyDiagnostics (directives, diag, mgr);

    if (tokensOk && diagsOk) {
        std::cout << std::format ("[PASSED] {}\n", testFilePath);
        return 0;
    }
    std::cout << std::format ("[FAILED] {}\n", testFilePath);
    return 1;
}
