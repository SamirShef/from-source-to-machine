#include "pebble/basic/pos.h"
#include "pebble/basic/source_mgr.h"
#include "pebble/diagnostic/codes.h"
#include "pebble/diagnostic/colors.h"
#include "pebble/diagnostic/engine.h"
#include "pebble/diagnostic/span.h"
#include "pebble/lexer/lexer.h"
#include <cstring>
#include <format>

using namespace pebble;

struct Env {
    bool DumpTokens{};
};

void
ParseCommands (int argc, char **argv, Env &env);

int
main (int argc, char **argv) {
    Env env;
    ParseCommands (argc, argv, env);
    EnableVirtualTerminalProcessing ();
    basic::SourceMgr mgr;
    const auto      *content = "let x: int32 = \"Hello world!\";\n";
    mgr.AddFile ("main.pebble", content);
    diagnostic::DiagnosticEngine diag (mgr);

    Lexer lex;
    if (env.DumpTokens) {
        std::cout << "==== TOKENS ====\n";
    }
    while (true) {
        Token tok = lex.NextToken ();
        if (env.DumpTokens) {
            std::cout << std::format ("[{:02}]", (int) tok.Kind) << " '" << tok.Val
                      << "'\n";
        }
        if (tok.Kind == TokenKind::Eof) {
            break;
        }
    }

    diag.Render ();
    return 0;
}

void
ParseCommands (int argc, char **argv, Env &env) {
    for (int i = 0; i < argc; ++i) {
        if (std::strcmp (argv[i], "--dump-tokens") == 0) {
            env.DumpTokens = true;
        }
    }
}
