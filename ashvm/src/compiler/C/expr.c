#include <stdio.h>
#include <stdlib.h>
#include "compiler/internal.h"
#include "diagnostics/diagnostics.h"

// Precedence climbing: expression -> logical_or -> logical_and ->
// comparison -> additive -> term -> unary -> postfix -> primary.

static void primary(void) {
    if (current.type == TOKEN_FN) { lambda_literal(); return; }
    if (current.type == TOKEN_NUMBER) {
        double v = strtod(current.start, NULL);
        advance_token();
        emit_constant(vm_num(v));
        return;
    }
    if (current.type == TOKEN_STRING) {
        char *s = vm_copy_string_escaped(current.start, current.length);
        advance_token();
        emit_constant(vm_str(s));
        return;
    }
    if (current.type == TOKEN_LBRACKET) {
        advance_token();
        int count = 0;
        if (current.type != TOKEN_RBRACKET) {
            expression(); count++;
            while (current.type == TOKEN_COMMA) { advance_token(); expression(); count++; }
        }
        expect(TOKEN_RBRACKET, "expected ']' after array literal");
        emit_op(OP_ARRAY);
        emit((uint8_t)count);
        return;
    }
    if (current.type == TOKEN_LBRACE) {
        advance_token();
        int n = 0;
        if (current.type != TOKEN_RBRACE) {
            for (;;) {
                if (current.type != TOKEN_STRING) { diagnostics_report("error", "expected string key in map literal", current.start, current.length); exit(1); }
                char *key = vm_copy_string_escaped(current.start, current.length);
                advance_token();
                expect(TOKEN_COLON, "expected ':' after map key");
                emit_constant(vm_str(key));
                expression();
                n++;
                if (current.type == TOKEN_COMMA) { advance_token(); continue; }
                break;
            }
        }
        expect(TOKEN_RBRACE, "expected '}' after map literal");
        emit_op(OP_MAP);
        emit((uint8_t)n);
        return;
    }
    if (current.type == TOKEN_IDENTIFIER) {
        Token id = current;
        advance_token();

        if (current.type == TOKEN_DOT) {
            advance_token();
            expect(TOKEN_IDENTIFIER, "expected name after '.'");
            Token member = previous;

            char combined[128];
            int combined_len = snprintf(combined, sizeof(combined), "%.*s.%.*s",
                                          id.length, id.start, member.length, member.start);

            if (current.type == TOKEN_LPAREN) {
                advance_token();
                emit_call_named(combined, combined_len);
                return;
            }

            VMConstant *k = resolve_constant(combined, combined_len);
            if (k != NULL) { codegen_constant_value(k); return; }

            diagnostics_report("error", "undefined namespaced function or constant", id.start, id.length);
            exit(1);
        }

        if (current.type == TOKEN_LPAREN) {
            advance_token();
            emit_call(id);
            return;
        }
        int slot = resolve_local(id.start, id.length);
        if (slot != -1) { emit2(OP_GET_LOCAL, (uint8_t)slot); return; }
        int fn_idx = find_function_in_context(id.start, id.length);
        if (fn_idx != -1) { emit_constant(vm_function_val(fn_idx)); return; }

        // Not a function either -- if we're compiling inside an imported
        // file's own namespace context, a bare reference may be that same
        // file's own top-level constant.
        const char *ns; int ns_len;
        get_import_namespace(&ns, &ns_len);
        if (ns_len > 0) {
            char combined[128];
            int combined_len = snprintf(combined, sizeof(combined), "%.*s.%.*s", ns_len, ns, id.length, id.start);
            VMConstant *k = resolve_constant(combined, combined_len);
            if (k != NULL) { codegen_constant_value(k); return; }
        }

        char msg[128];
        snprintf(msg, sizeof(msg), "undefined variable: %.*s", id.length, id.start);
        diagnostics_report("error", msg, id.start, id.length);
        exit(1);
    }
    if (current.type == TOKEN_LPAREN) {
        advance_token();
        expression();
        expect(TOKEN_RPAREN, "expected ')' after expression");
        return;
    }
    diagnostics_report("error", "expected value", current.start, current.length);
    exit(1);
}

static void postfix(void) {
    primary();
    while (current.type == TOKEN_LBRACKET) {
        advance_token();
        expression();
        expect(TOKEN_RBRACKET, "expected ']' after index");
        emit_op(OP_INDEX_GET);
    }
}

static void unary(void) {
    if (current.type == TOKEN_BANG) { advance_token(); unary(); emit_op(OP_NOT); return; }
    if (current.type == TOKEN_MINUS) { advance_token(); unary(); emit_op(OP_NEG); return; }
    postfix();
}

static void term(void) {
    unary();
    while (current.type == TOKEN_STAR || current.type == TOKEN_SLASH || current.type == TOKEN_PERCENT) {
        TokenType op = current.type;
        advance_token();
        unary();
        emit_op(op == TOKEN_STAR ? OP_MUL : op == TOKEN_SLASH ? OP_DIV : OP_MOD);
    }
}

static void additive(void) {
    term();
    while (current.type == TOKEN_PLUS || current.type == TOKEN_MINUS) {
        TokenType op = current.type;
        advance_token();
        term();
        emit_op(op == TOKEN_PLUS ? OP_ADD : OP_SUB);
    }
}

static void comparison(void) {
    additive();
    if (current.type == TOKEN_EQUAL_EQUAL || current.type == TOKEN_BANG_EQUAL ||
        current.type == TOKEN_LESS || current.type == TOKEN_LESS_EQUAL ||
        current.type == TOKEN_GREATER || current.type == TOKEN_GREATER_EQUAL) {
        TokenType op = current.type;
        advance_token();
        additive();
        switch (op) {
            case TOKEN_EQUAL_EQUAL: emit_op(OP_EQ); break;
            case TOKEN_BANG_EQUAL: emit_op(OP_NEQ); break;
            case TOKEN_LESS: emit_op(OP_LT); break;
            case TOKEN_LESS_EQUAL: emit_op(OP_LE); break;
            case TOKEN_GREATER: emit_op(OP_GT); break;
            case TOKEN_GREATER_EQUAL: emit_op(OP_GE); break;
            default: break;
        }
    }
}

static void logical_and(void) {
    comparison();
    while (current.type == TOKEN_AND) { advance_token(); comparison(); emit_op(OP_AND); }
}
static void logical_or(void) {
    logical_and();
    while (current.type == TOKEN_OR) { advance_token(); logical_and(); emit_op(OP_OR); }
}
void expression(void) { logical_or(); }
