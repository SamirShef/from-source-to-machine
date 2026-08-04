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

struct ExpectedToken {
    std::uint32_t TargetLine;
    std::string   KindStr;
    std::string   Value;
    bool          HasValueCheck{};
    std::uint32_t DirectiveLine;
    bool          Matched{};
};

static std::vector<ExpectedToken>
ParseDirectives (const std::string &source) {
    std::vector<ExpectedToken> expected;
    std::istringstream         stream (source);
    std::string                line;
    std::uint32_t              currentLine = 1;

    static const std::regex DIRECTIVE_REGEX (
        "(?://|/"
        "\\*)\\s*expect-token(?:@([+-]?\\d+))?\\s*:\\s*([A-Za-z0-9_]+)(?:\\s+\"([^\"]*)"
        "\")?");

    while (std::getline (stream, line)) {
        std::smatch                 match;
        std::string::const_iterator searchStart (line.cbegin ());

        while (std::regex_search (searchStart, line.cend (), match, DIRECTIVE_REGEX)) {
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

            expected.push_back (exp);
            searchStart = match[0].second;
        }
        ++currentLine;
    }

    return expected;
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

    auto expectedTokens = ParseDirectives (sourceContent);

    basic::SourceMgr             mgr;
    diagnostic::DiagnosticEngine diag (mgr);
    std::vector<Token>           actualTokens;

    if (!RunCompilerDriver (testFilePath, mgr, diag, actualTokens)) {
        return 1;
    }

    if (VerifyTokens (expectedTokens, actualTokens, mgr)) {
        std::cout << std::format ("[PASSED] {}\n", testFilePath);
        return 0;
    }
    std::cout << std::format ("[FAILED] {}\n", testFilePath);
    return 1;
}
