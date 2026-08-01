#include "pebble/basic/pos.h"
#include "pebble/basic/source_mgr.h"
#include "pebble/diagnostic/codes.h"
#include "pebble/diagnostic/colors.h"
#include "pebble/diagnostic/engine.h"
#include "pebble/diagnostic/span.h"

using namespace pebble;

int
main () {
    EnableVirtualTerminalProcessing ();
    basic::SourceMgr mgr;
    const auto      *content = "let x: int32 = \"Hello world!\";\n";
    mgr.AddFile ("main.pebble", content);
    diagnostic::DiagnosticEngine diag (mgr);

    auto span = diagnostic::Span (basic::Pos (15, 0), basic::Pos (29, 0));
    diag.Report (
            diagnostic::DiagCode::EExpectedExpr,
            "mismatched types",
            diagnostic::DiagSeverity::Error)
        .AddAnnotation (span, "expected 'int32', found 'string'");

    diag.Render ();
    return 0;
}
