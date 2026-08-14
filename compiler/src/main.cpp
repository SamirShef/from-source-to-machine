#include "pebble/basic/loc.h"
#include "pebble/basic/pos.h"
#include "pebble/basic/source_mgr.h"
#include "pebble/diagnostic/colors.h"
#include "pebble/diagnostic/engine.h"
#include "pebble/diagnostic/span.h"
#include "pebble/lexer/lexer.h"
#include <cstring>
#include <format>
#include <fstream>
#include <sstream>
#include <vector>

using namespace pebble;

struct Env {
    bool                     DumpTokens{};
    std::vector<std::string> InputFiles;
};

void
ParseCommands (int argc, char **argv, Env &env);

std::string
LocToString (basic::Loc loc);

bool
CompileFile (
    const std::string            &path,
    basic::SourceMgr             &mgr,
    diagnostic::DiagnosticEngine &diag,
    Env                          &env);

int
main (int argc, char **argv) {
    Env env;
    ParseCommands (argc, argv, env);

    if (env.InputFiles.empty ()) {
        std::cerr << "Usage: " << argv[0] << " <input files> [options]\n";
        return 1;
    }

    EnableVirtualTerminalProcessing ();
    basic::SourceMgr             mgr;
    diagnostic::DiagnosticEngine diag (mgr);
    bool                         ok = true;
    for (const auto &path : env.InputFiles) {
        if (!CompileFile (path, mgr, diag, env)) {
            ok = false;
        }
    }
    return static_cast<int> (!ok);
}

void
ParseCommands (int argc, char **argv, Env &env) {
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp (argv[i], "--dump-tokens") == 0) {
            env.DumpTokens = true;
        } else {
            env.InputFiles.emplace_back (argv[i]);
        }
    }
}

std::string
LocToString (basic::Loc loc) {
    return std::to_string (loc.Line) + ':' + std::to_string (loc.Col);
}

bool
CompileFile (
    const std::string            &path,
    basic::SourceMgr             &mgr,
    diagnostic::DiagnosticEngine &diag,
    Env                          &env) {
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
    if (env.DumpTokens) {
        std::cout << "==== TOKENS ====\n";
    }
    while (true) {
        Token tok = lex.NextToken ();
        if (env.DumpTokens) {
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
