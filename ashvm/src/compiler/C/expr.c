#include <stdio.h>
#include <stdlib.h>
#include "compiler/H/internal.h"
#include "diagnostics/H/diagnostics.h"

// Precedence climbing: expression -> logical_or -> logical_and ->
// comparison -> additive -> term -> unary -> postfix -> primary.

static void primary(void) {
    if (current.type == TOKEN_FORGE) { lambda_literal(); return; }
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
    if (current.type == TOKEN_YES || current.type == TOKEN_NO) {
        int v = current.type == TOKEN_YES;
        advance_token();
        emit_constant(vm_bool(v));
        return;
    }
    if (current.type == TOKEN_NONE) {
        advance_token();
        emit_constant(vm_nil());
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

        // `var.field` on a variable (not a namespace) is var["field"]; on an error
        // string it is the string itself, so `e.message` works for built-in errors
        // and for raised maps alike.
        if (current.type == TOKEN_DOT && resolve_local(id.start, id.length) != -1) {
            emit2(OP_GET_LOCAL, (uint8_t)resolve_local(id.start, id.length));
            while (current.type == TOKEN_DOT) {
                advance_token();
                expect(TOKEN_IDENTIFIER, "expected field name after '.'");
                emit_constant(vm_str(vm_copy_string_escaped(previous.start, previous.length)));
                emit_op(OP_INDEX_GET);
            }
            return;
        }

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
            int state = module_state_resolve_qualified(combined, combined_len);
            if (state != -1) { emit2(OP_GET_GLOBAL, (uint8_t)state); return; }

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

        int state = module_state_resolve(id.start, id.length);
        if (state != -1) { emit2(OP_GET_GLOBAL, (uint8_t)state); return; }
        if (emit_builtin_value(id)) return;

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
    if (current.type == TOKEN_TILDE) { advance_token(); unary(); emit_op(OP_BNOT); return; }
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

// Bitwise operators (& | ^ << >>), one flat precedence level (left to
// right) sitting between comparison and logical_and -- not real C
// precedence (which splits shift/bitand/bitxor/bitor into four separate
// levels), a deliberate simplification matching kiln's own choice here.
// Operands are truncated to int64 in the VM (do_BAND etc. in
// vm/C/dispatch.c), same "assume NUMBER" scope limit as -, *, /, and the
// comparison operators.
static void bitwise(void) {
    comparison();
    while (current.type == TOKEN_AMP || current.type == TOKEN_PIPE ||
           current.type == TOKEN_CARET || current.type == TOKEN_SHL || current.type == TOKEN_SHR) {
        TokenType op = current.type;
        advance_token();
        comparison();
        switch (op) {
            case TOKEN_AMP: emit_op(OP_BAND); break;
            case TOKEN_PIPE: emit_op(OP_BOR); break;
            case TOKEN_CARET: emit_op(OP_BXOR); break;
            case TOKEN_SHL: emit_op(OP_SHL); break;
            default: emit_op(OP_SHR); break;
        }
    }
}

// `and` / `or` short-circuit: the right operand only runs when the left
// doesn't already decide the result. Built from the same OP_JUMP_IF_FALSE
// the ternary uses; the right operand is normalized to 0/1 by OP_AND/OP_OR
// against a constant, so the result is the same 0/1 the old eager ops gave.
static void logical_and(void) {
    bitwise();
    while (current.type == TOKEN_AND) {
        advance_token();
        int lhs_false = emit_jump(OP_JUMP_IF_FALSE);
        bitwise();
        emit_constant(vm_num(1));
        emit_op(OP_AND);
        int end = emit_jump(OP_JUMP);
        patch_jump(lhs_false);
        emit_constant(vm_bool(0));
        patch_jump(end);
    }
}
static void logical_or(void) {
    logical_and();
    while (current.type == TOKEN_OR) {
        advance_token();
        int try_rhs = emit_jump(OP_JUMP_IF_FALSE);
        emit_constant(vm_bool(1));
        int end = emit_jump(OP_JUMP);
        patch_jump(try_rhs);
        logical_and();
        emit_constant(vm_num(0));
        emit_op(OP_OR);
        patch_jump(end);
    }
}

// a ? b : c  --  lowest precedence, sits above logical_or. Right-associative
// via the recursive expression() calls for both branches (so
// `a ? b : c ? d : e` parses as `a ? b : (c ? d : e)`), same jump-patch
// shape as given/during's own truthy-test-then-jump pattern, just as an
// expression instead of a statement. OP_JUMP_IF_FALSE pops the condition
// itself, matching how compile_given_stmt already uses it.
void expression(void) {
    logical_or();
    if (current.type == TOKEN_QUESTION) {
        advance_token();
        int else_jump = emit_jump(OP_JUMP_IF_FALSE);
        expression(); // true branch
        int end_jump = emit_jump(OP_JUMP);
        expect(TOKEN_COLON, "expected ':' in ternary expression");
        patch_jump(else_jump);
        expression(); // false branch
        patch_jump(end_jump);
    }
}
