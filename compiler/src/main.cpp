#include "pebble/basic/loc.h"
#include "pebble/basic/options.h"
#include "pebble/basic/pos.h"
#include "pebble/basic/source_mgr.h"
#include "pebble/cl/parser.h"
#include "pebble/diagnostic/colors.h"
#include "pebble/diagnostic/engine.h"
#include "pebble/diagnostic/span.h"
#include "pebble/lexer/lexer.h"
#include <format>
#include <fstream>
#include <sstream>

using namespace pebble;

std::string
LocToString (basic::Loc loc);

bool
CompileFile (
    const std::string &path, basic::SourceMgr &mgr, diagnostic::DiagnosticEngine &diag);

int
main (int argc, char **argv) {
    if (!cl::ParseCommandLineOptions (argc, argv)) {
        return 1;
    }

    EnableVirtualTerminalProcessing ();
    basic::SourceMgr             mgr;
    diagnostic::DiagnosticEngine diag (mgr);
    bool                         ok = true;
    for (const auto &path : InputFiles.Get ()) {
        if (!CompileFile (path, mgr, diag)) {
            ok = false;
        }
    }
    return ok ? 0 : 1;
}

std::string
LocToString (basic::Loc loc) {
    return std::to_string (loc.Line) + ':' + std::to_string (loc.Col);
}

bool
CompileFile (
    const std::string &path, basic::SourceMgr &mgr, diagnostic::DiagnosticEngine &diag) {
    std::cout << "Compilation file " << path << "...\n";
    std::ifstream file (path);
    if (!file.is_open ()) {
        std::cerr << path << ": Error opening file!\n";
        return false;
    }
    std::stringstream buffer;
    buffer << file.rdbuf ();
    auto  fileId = mgr.AddFile (path, buffer.str ());
    Lexer lex (diag, fileId);
    if (DumpTokens) {
        std::cout << "==== TOKENS ====\n";
    }
    while (true) {
        Token tok = lex.NextToken ();
        if (DumpTokens) {
            auto startLoc = mgr.FindLoc (tok.Span.Start);
            auto endLoc   = mgr.FindLoc (tok.Span.End);
            std::cout << std::format ("[{}]", TokenKindToString (tok.Kind)) << " '"
                      << tok.Val << "' (" << LocToString (startLoc) << '-'
                      << LocToString (endLoc) << ")\n";
        }
        if (tok.Kind == TokenKind::Eof) {
            break;
        }
    }

    diag.Render ();
    return !diag.HasErrors ();
}
