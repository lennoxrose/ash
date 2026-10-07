#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "compiler/H/internal.h"
#include "builtins/H/builtins.h"
#include "diagnostics/H/diagnostics.h"
#include "compiler/H/loop_stack.h"

void emit_call(Token id) {
    int builtin_id = vm_builtin_lookup(id.start, id.length);
    int fn_idx = (builtin_id == -1) ? find_function_in_context(id.start, id.length) : -1;
    int local_slot = (builtin_id == -1 && fn_idx == -1) ? resolve_local(id.start, id.length) : -1;

    int argc = 0;
    if (current.type != TOKEN_RPAREN) {
        expression(); argc++;
        while (current.type == TOKEN_COMMA) { advance_token(); expression(); argc++; }
    }
    expect(TOKEN_RPAREN, "expected ')' after arguments");

    if (builtin_id != -1) {
        emit_op(OP_CALL_BUILTIN);
        emit((uint8_t)builtin_id);
        emit((uint8_t)argc);
    } else if (fn_idx != -1) {
        if (argc != vm_functions[fn_idx].arity) {
            char msg[128];
            snprintf(msg, sizeof(msg), "function %.*s expected %d args, got %d", id.length, id.start, vm_functions[fn_idx].arity, argc);
            diagnostics_report("error", msg, id.start, id.length);
            exit(1);
        }
        emit_op(OP_CALL);
        emit((uint8_t)fn_idx);
        emit((uint8_t)argc);
    } else if (local_slot != -1) {
        emit2(OP_GET_LOCAL, (uint8_t)local_slot);
        emit_op(OP_CALL_VALUE);
        emit((uint8_t)argc);
    } else {
        char msg[128];
        snprintf(msg, sizeof(msg), "undefined function: %.*s", id.length, id.start);
        diagnostics_report("error", msg, id.start, id.length);
        exit(1);
    }
}

// Emits a call to an already-fully-qualified function name (e.g.
// "ns.func"), used by expr.c's `ns.func(...)` dispatch -- unlike
// emit_call(), this never falls back to builtins/locals/context-namespace
// lookup, since a namespaced call can only ever mean an imported file's
// function.
void emit_call_named(const char *name, int len) {
    int fn_idx = find_function(name, len); // already fully-qualified

    int argc = 0;
    if (current.type != TOKEN_RPAREN) {
        expression(); argc++;
        while (current.type == TOKEN_COMMA) { advance_token(); expression(); argc++; }
    }
    expect(TOKEN_RPAREN, "expected ')' after arguments");

    if (fn_idx == -1) {
        char msg[128];
        snprintf(msg, sizeof(msg), "undefined function: %.*s", len, name);
        diagnostics_report("error", msg, current.start, current.length);
        exit(1);
    }
    if (argc != vm_functions[fn_idx].arity) {
        char msg[128];
        snprintf(msg, sizeof(msg), "function %.*s expected %d args, got %d", len, name, vm_functions[fn_idx].arity, argc);
        diagnostics_report("error", msg, current.start, current.length);
        exit(1);
    }
    emit_op(OP_CALL);
    emit((uint8_t)fn_idx);
    emit((uint8_t)argc);
}

// forge(params) { body } as an expression. Captures the ENTIRE enclosing
// scope's locals by value at creation time (same rule as ashc's closures
// used to have). The captured variables get their own reserved local slots
// in the lambda's own chunk, placed right after its parameters, so the
// compiled body can reference them via the normal resolve_local()
// mechanism just like any other local.
void lambda_literal(void) {
    advance_token();
    expect(TOKEN_LPAREN, "expected '(' after 'forge'");

    char namebuf[32];
    int namelen = snprintf(namebuf, sizeof(namebuf), "__lambda_%d", lambda_counter++);
    if (namelen > (int)sizeof(namebuf) - 1) namelen = (int)sizeof(namebuf) - 1;

    if (vm_function_count >= MAX_VM_FUNCS) { diagnostics_report("error", "too many functions", current.start, current.length); exit(1); }
    int fn_idx = vm_function_count++;
    memcpy(vm_functions[fn_idx].name, namebuf, namelen);
    vm_functions[fn_idx].name[namelen] = '\0';
    vm_functions[fn_idx].arity = 0;
    chunk_init(&vm_functions[fn_idx].chunk);

    char param_names[8][64];
    int param_lens[8];
    int argc = 0;
    if (current.type != TOKEN_RPAREN) {
        expect(TOKEN_IDENTIFIER, "expected parameter name");
        memcpy(param_names[argc], previous.start, previous.length);
        param_lens[argc] = previous.length;
        argc++;
        while (current.type == TOKEN_COMMA) {
            advance_token();
            expect(TOKEN_IDENTIFIER, "expected parameter name");
            memcpy(param_names[argc], previous.start, previous.length);
            param_lens[argc] = previous.length;
            argc++;
        }
    }
    expect(TOKEN_RPAREN, "expected ')' after parameters");
    vm_functions[fn_idx].arity = argc;

    int capture_count = local_count;
    char captured_names[MAX_VM_LOCALS][64];
    memcpy(captured_names, local_names, sizeof(local_names));

    Chunk *outer_chunk = chunk;
    char outer_locals[MAX_VM_LOCALS][64];
    int outer_local_count = local_count;
    memcpy(outer_locals, local_names, sizeof(local_names));
    int outer_loop_depth;
    loop_depth_save(&outer_loop_depth);
    loop_depth_reset();

    chunk = &vm_functions[fn_idx].chunk;
    local_count = 0;
    for (int i = 0; i < argc; i++) declare_local(param_names[i], param_lens[i]);
    for (int i = 0; i < capture_count; i++) declare_local(captured_names[i], (int)strlen(captured_names[i]));

    block();
    emit_constant(vm_num(0));
    emit_op(OP_RETURN);

    chunk = outer_chunk;
    local_count = outer_local_count;
    memcpy(local_names, outer_locals, sizeof(local_names));
    loop_depth_restore(outer_loop_depth);

    emit_op(OP_MAKE_CLOSURE);
    emit((uint8_t)fn_idx);
    emit((uint8_t)capture_count);
}

// Tries to compile `identifier CMP (number|identifier)` directly into a
// fused OP_CMP_JUMP, used by if/while conditions. Returns the bytecode
// offset to patch_jump() once the branch target is known, or -1 if the
// condition didn't match the fusable shape (caller falls back to a plain
// expression() + OP_JUMP_IF_FALSE).
int try_fuse_condition(TokenType terminator) {
    if (current.type != TOKEN_IDENTIFIER) return -1;
    Token save = current;
    Token id = current;
    advance_token();

    int cmp_code;
    switch (current.type) {
        case TOKEN_LESS: cmp_code = 0; break;
        case TOKEN_LESS_EQUAL: cmp_code = 1; break;
        case TOKEN_GREATER: cmp_code = 2; break;
        case TOKEN_GREATER_EQUAL: cmp_code = 3; break;
        case TOKEN_EQUAL_EQUAL: cmp_code = 4; break;
        case TOKEN_BANG_EQUAL: cmp_code = 5; break;
        default:
            lexer_init(save.start + save.length); current = save; return -1;
    }
    advance_token();

    int slot_a = resolve_local(id.start, id.length);
    if (slot_a == -1) { lexer_init(save.start + save.length); current = save; return -1; }

    uint8_t flags = (uint8_t)cmp_code;
    uint8_t b_operand;
    if (current.type == TOKEN_NUMBER) {
        double v = strtod(current.start, NULL);
        b_operand = (uint8_t)chunk_add_constant(chunk, vm_num(v));
        flags |= 0x80;
        advance_token();
    } else if (current.type == TOKEN_IDENTIFIER) {
        int slot_b = resolve_local(current.start, current.length);
        if (slot_b == -1) { lexer_init(save.start + save.length); current = save; return -1; }
        b_operand = (uint8_t)slot_b;
        advance_token();
    } else {
        lexer_init(save.start + save.length); current = save; return -1;
    }

    if (current.type != terminator) {
        lexer_init(save.start + save.length); current = save; return -1;
    }

    emit_op(OP_CMP_JUMP);
    emit((uint8_t)slot_a);
    emit(flags);
    emit(b_operand);
    int patch_loc = chunk->count;
    emit(0xff); emit(0xff);
    return patch_loc;
}
