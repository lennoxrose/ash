#include <stdio.h>
#include <stdlib.h>
#include "compiler/H/internal.h"
#include "compiler/H/compiler.h"
#include "diagnostics/H/diagnostics.h"

// Forward declarations. Before a file is compiled, scan its tokens for named
// `forge name(a, b)` declarations and reserve each name with its arity, so an
// earlier function (or top-level code) can call a later one, and two functions
// can call each other. A declaration still has to appear somewhere in the
// program; check_all_functions_defined() reports a name that was reserved but
// never defined. Lambdas (`forge (x) { ... }`) have no name and are skipped.
void prescan_functions(const char *source) {
    LexerState saved = lexer_save_state();
    lexer_init(source);
    Token t = lexer_next_token();
    while (t.type != TOKEN_EOF) {
        if (t.type == TOKEN_FORGE) {
            Token name = lexer_next_token();
            if (name.type != TOKEN_IDENTIFIER) { t = name; continue; }
            Token paren = lexer_next_token();
            if (paren.type != TOKEN_LPAREN) { t = paren; continue; }
            int arity = 0;
            t = lexer_next_token();
            if (t.type != TOKEN_RPAREN) {
                arity = 1;
                while (t.type != TOKEN_RPAREN && t.type != TOKEN_EOF) {
                    if (t.type == TOKEN_COMMA) arity++;
                    t = lexer_next_token();
                }
            }
            predeclare_function_in_context(name.start, name.length, arity);
        }
        if (t.type != TOKEN_EOF) t = lexer_next_token();
    }
    lexer_restore_state(saved);
}

void check_all_functions_defined(void) {
    for (int i = 0; i < vm_function_count; i++) {
        if (!vm_functions[i].defined) {
            char msg[128];
            snprintf(msg, sizeof(msg), "undefined function: %s", vm_functions[i].name);
            diagnostics_report("error", msg, current.start, current.length);
            exit(1);
        }
    }
}
