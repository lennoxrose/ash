#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <math.h>
#include "vm/H/vm.h"
#include "vm/H/state.h"
#include "compiler/H/compiler.h"
#include "builtins/H/builtins.h"
#include "value/H/hashmap.h"

// The dispatch loop stays one function despite exceeding this project's
// usual 200-line target: computed-goto (`&&label` / `goto *table[i]`)
// requires every label to live in the same function scope, so splitting it
// by opcode group isn't possible without giving up the dispatch technique
// that makes this VM fast in the first place. Everything that CAN live
// outside it (error handling, the public call/run API, shared state) does,
// in vm_errors.c / vm_api.c / vm_state.c.
void vm_execute(int stop_at_frame_count) {
    CallFrame *volatile frame = &frames[frame_count - 1];

    static void *dispatch_table[] = {
        &&do_CONST, &&do_ADD, &&do_SUB, &&do_MUL, &&do_DIV, &&do_MOD, &&do_NEG,
        &&do_NOT, &&do_EQ, &&do_NEQ, &&do_LT, &&do_LE, &&do_GT, &&do_GE,
        &&do_AND, &&do_OR,
        &&do_PRINT, &&do_POP,
        &&do_GET_LOCAL, &&do_SET_LOCAL,
        &&do_JUMP, &&do_JUMP_IF_FALSE, &&do_LOOP,
        &&do_CALL, &&do_RETURN,
        &&do_ACC_LOCAL, &&do_CMP_JUMP,
        &&do_ARRAY, &&do_INDEX_GET, &&do_INDEX_SET,
        &&do_MAP, &&do_CALL_VALUE, &&do_CALL_BUILTIN,
        &&do_MAKE_CLOSURE,
        &&do_TRY_PUSH, &&do_TRY_POP,
        &&do_RAISE,
        &&do_BAND, &&do_BOR, &&do_BXOR, &&do_BNOT, &&do_SHL, &&do_SHR,
        &&do_TRUNC_LOCALS
    };

    #define DISPATCH() \
        do { \
            if (frame_count == 1 && frame->ip >= frame->chunk->code + frame->chunk->count) return; \
            size_t __instr_off = (size_t)(frame->ip - frame->chunk->code); \
            current_pos = frame->chunk->pos[__instr_off]; \
            current_poslen = frame->chunk->poslen[__instr_off]; \
            goto *dispatch_table[*frame->ip++]; \
        } while (0)

    DISPATCH();

    do_CONST: { uint8_t idx = *frame->ip++; *stack_top++ = frame->chunk->constants[idx]; DISPATCH(); }
    do_ADD: {
        VMValue b = *--stack_top;
        VMValue *a = stack_top - 1;
        if (a->type == VM_STR && b.type == VM_STR) *a = vm_str(vm_concat_strings(a->str, b.str));
        else *a = vm_num(require_num(*a, "'+'") + require_num(b, "'+'"));
        DISPATCH();
    }
    do_SUB: { VMValue b = *--stack_top; VMValue *a = stack_top - 1; *a = vm_num(require_num(*a, "'-'") - require_num(b, "'-'")); DISPATCH(); }
    do_MUL: { VMValue b = *--stack_top; VMValue *a = stack_top - 1; *a = vm_num(require_num(*a, "'*'") * require_num(b, "'*'")); DISPATCH(); }
    do_DIV: { VMValue b = *--stack_top; VMValue *a = stack_top - 1; *a = vm_num(require_num(*a, "'/'") / require_num(b, "'/'")); DISPATCH(); }
    do_MOD: { VMValue b = *--stack_top; VMValue *a = stack_top - 1; *a = vm_num(fmod(require_num(*a, "'%'"), require_num(b, "'%'"))); DISPATCH(); }
    do_NEG: { stack_top[-1] = vm_num(-require_num(stack_top[-1], "unary '-'")); DISPATCH(); }
    do_NOT: { stack_top[-1] = vm_num(require_num(stack_top[-1], "'!'") == 0 ? 1 : 0); DISPATCH(); }
    do_EQ: {
        VMValue b = *--stack_top; VMValue *a = stack_top - 1;
        int eq;
        if (a->type == VM_STR && b.type == VM_STR) eq = strcmp(a->str, b.str) == 0;
        else if (a->type == VM_NUM && b.type == VM_NUM) eq = a->number == b.number;
        else if (a->type == VM_NIL && b.type == VM_NIL) eq = 1;
        else eq = 0;
        *a = vm_num(eq);
        DISPATCH();
    }
    do_NEQ: {
        VMValue b = *--stack_top; VMValue *a = stack_top - 1;
        int eq;
        if (a->type == VM_STR && b.type == VM_STR) eq = strcmp(a->str, b.str) == 0;
        else if (a->type == VM_NUM && b.type == VM_NUM) eq = a->number == b.number;
        else if (a->type == VM_NIL && b.type == VM_NIL) eq = 1;
        else eq = 0;
        *a = vm_num(!eq);
        DISPATCH();
    }
    do_LT: { VMValue b = *--stack_top; VMValue *a = stack_top - 1; *a = vm_num(require_num(*a, "comparison") < require_num(b, "comparison")); DISPATCH(); }
    do_LE: { VMValue b = *--stack_top; VMValue *a = stack_top - 1; *a = vm_num(require_num(*a, "comparison") <= require_num(b, "comparison")); DISPATCH(); }
    do_GT: { VMValue b = *--stack_top; VMValue *a = stack_top - 1; *a = vm_num(require_num(*a, "comparison") > require_num(b, "comparison")); DISPATCH(); }
    do_GE: { VMValue b = *--stack_top; VMValue *a = stack_top - 1; *a = vm_num(require_num(*a, "comparison") >= require_num(b, "comparison")); DISPATCH(); }
    do_AND: { VMValue b = *--stack_top; VMValue *a = stack_top - 1; *a = vm_num((truthy(*a, "'&&'") && truthy(b, "'&&'")) ? 1 : 0); DISPATCH(); }
    do_OR: { VMValue b = *--stack_top; VMValue *a = stack_top - 1; *a = vm_num((truthy(*a, "'||'") || truthy(b, "'||'")) ? 1 : 0); DISPATCH(); }
    do_PRINT: { VMValue v = *--stack_top; vm_print_value(v); printf("\n"); DISPATCH(); }
    do_POP: { --stack_top; DISPATCH(); }
    do_GET_LOCAL: { uint8_t slot = *frame->ip++; *stack_top++ = frame->slots[slot]; DISPATCH(); }
    do_SET_LOCAL: { uint8_t slot = *frame->ip++; frame->slots[slot] = stack_top[-1]; DISPATCH(); }
    do_JUMP: {
        uint16_t off = (uint16_t)((frame->ip[0] << 8) | frame->ip[1]);
        frame->ip += 2; frame->ip += off; DISPATCH();
    }
    do_JUMP_IF_FALSE: {
        uint16_t off = (uint16_t)((frame->ip[0] << 8) | frame->ip[1]);
        frame->ip += 2;
        VMValue cond = *--stack_top;
        if (!truthy(cond, "condition")) frame->ip += off;
        DISPATCH();
    }
    do_LOOP: {
        uint16_t off = (uint16_t)((frame->ip[0] << 8) | frame->ip[1]);
        frame->ip += 2; frame->ip -= off; DISPATCH();
    }
    do_CALL: {
        uint8_t func_idx = *frame->ip++;
        uint8_t argc = *frame->ip++;
        VMFunction *fn = &vm_functions[func_idx];
        CallFrame *volatile newf = &frames[frame_count++];
        newf->chunk = &fn->chunk;
        newf->ip = fn->chunk.code;
        newf->slots = stack_top - argc;
        frame = newf;
        DISPATCH();
    }
    do_CALL_VALUE: {
        uint8_t argc = *frame->ip++;
        VMValue callable = *--stack_top;
        VMFunction *fn;
        int extra = 0;
        if (callable.type == VM_FUNCTION) {
            fn = &vm_functions[(int)callable.number];
        } else if (callable.type == VM_CLOSURE) {
            fn = &vm_functions[callable.closure->function_index];
            extra = callable.closure->capture_count;
            for (int i = 0; i < extra; i++) *stack_top++ = callable.closure->captured[i];
        } else {
            type_error("call");
            fn = NULL;
        }
        CallFrame *volatile newf = &frames[frame_count++];
        newf->chunk = &fn->chunk;
        newf->ip = fn->chunk.code;
        newf->slots = stack_top - argc - extra;
        frame = newf;
        DISPATCH();
    }
    do_CALL_BUILTIN: {
        uint8_t builtin_id = *frame->ip++;
        uint8_t argc = *frame->ip++;
        VMValue args[16];
        for (int i = argc - 1; i >= 0; i--) args[i] = *--stack_top;
        *stack_top++ = vm_call_builtin(builtin_id, args, argc);
        DISPATCH();
    }
    do_RETURN: {
        VMValue result = *--stack_top;
        stack_top = frames[frame_count - 1].slots;
        frame_count--;
        *stack_top++ = result;
        frame = &frames[frame_count - 1];
        if (stop_at_frame_count != 0 && frame_count == stop_at_frame_count) return;
        DISPATCH();
    }
    do_ACC_LOCAL: {
        uint8_t slot_a = *frame->ip++;
        uint8_t flags = *frame->ip++;
        uint8_t b_operand = *frame->ip++;
        VMValue b = (flags & 0x80) ? frame->chunk->constants[b_operand] : frame->slots[b_operand];
        int op = flags & 0x03;
        VMValue *a = &frame->slots[slot_a];
        if (op == 0) {
            if (a->type == VM_STR && b.type == VM_STR) *a = vm_str(vm_concat_strings(a->str, b.str));
            else *a = vm_num(require_num(*a, "'+'") + require_num(b, "'+'"));
        } else if (op == 1) {
            *a = vm_num(require_num(*a, "'-'") - require_num(b, "'-'"));
        } else {
            *a = vm_num(require_num(*a, "'*'") * require_num(b, "'*'"));
        }
        DISPATCH();
    }
    do_CMP_JUMP: {
        uint8_t slot_a = *frame->ip++;
        uint8_t flags = *frame->ip++;
        uint8_t b_operand = *frame->ip++;
        uint16_t off = (uint16_t)((frame->ip[0] << 8) | frame->ip[1]);
        frame->ip += 2;
        VMValue a = frame->slots[slot_a];
        VMValue b = (flags & 0x80) ? frame->chunk->constants[b_operand] : frame->slots[b_operand];
        int cmp_code = flags & 0x0F;
        int taken;
        if (cmp_code == 4 || cmp_code == 5) {
            int eq;
            if (a.type == VM_STR && b.type == VM_STR) eq = strcmp(a.str, b.str) == 0;
            else if (a.type == VM_NUM && b.type == VM_NUM) eq = a.number == b.number;
            else if (a.type == VM_NIL && b.type == VM_NIL) eq = 1;
            else eq = 0;
            taken = (cmp_code == 4) ? eq : !eq;
        } else {
            double av = require_num(a, "comparison");
            double bv = require_num(b, "comparison");
            switch (cmp_code) {
                case 0: taken = av < bv; break;
                case 1: taken = av <= bv; break;
                case 2: taken = av > bv; break;
                default: taken = av >= bv; break;
            }
        }
        if (!taken) frame->ip += off;
        DISPATCH();
    }
    do_ARRAY: {
        uint8_t n = *frame->ip++;
        VMArray *arr = vm_array_new();
        for (int i = 0; i < n; i++) vm_array_push(arr, stack_top[-n + i]);
        stack_top -= n;
        *stack_top++ = vm_array_val(arr);
        DISPATCH();
    }
    do_INDEX_GET: {
        VMValue idx = *--stack_top;
        VMValue arr = *--stack_top;
        if (arr.type == VM_ARRAY) {
            int64_t i = (int64_t)require_num(idx, "array index");
            if (i < 0 || i >= arr.array->count) vm_runtime_error("index out of bounds: %lld", (long long)i);
            *stack_top++ = arr.array->items[i];
        } else if (arr.type == VM_MAP) {
            if (idx.type != VM_STR) vm_runtime_error("map keys must be strings");
            VMValue out;
            if (!vm_map_get(arr.map, idx.str, &out)) vm_runtime_error("key not found: %s", idx.str);
            *stack_top++ = out;
        } else {
            type_error("indexing");
        }
        DISPATCH();
    }
    do_INDEX_SET: {
        VMValue value = *--stack_top;
        VMValue idx = *--stack_top;
        VMValue arr = *--stack_top;
        if (arr.type == VM_ARRAY) {
            int64_t i = (int64_t)require_num(idx, "array index");
            if (i < 0 || i >= arr.array->count) vm_runtime_error("index out of bounds: %lld", (long long)i);
            arr.array->items[i] = value;
        } else if (arr.type == VM_MAP) {
            if (idx.type != VM_STR) vm_runtime_error("map keys must be strings");
            char *key_copy = malloc(strlen(idx.str) + 1); strcpy(key_copy, idx.str);
            vm_map_set(arr.map, key_copy, value);
        } else {
            type_error("index-assign");
        }
        DISPATCH();
    }
    do_MAP: {
        uint8_t n = *frame->ip++;
        VMMap *m = vm_map_new();
        for (int i = 0; i < n; i++) {
            VMValue key = stack_top[-(2 * n) + 2 * i];
            VMValue val = stack_top[-(2 * n) + 2 * i + 1];
            char *key_copy = malloc(strlen(key.str) + 1); strcpy(key_copy, key.str);
            vm_map_set(m, key_copy, val);
        }
        stack_top -= 2 * n;
        *stack_top++ = vm_map_val(m);
        DISPATCH();
    }
    do_MAKE_CLOSURE: {
        uint8_t fn_idx = *frame->ip++;
        uint8_t cap_count = *frame->ip++;
        VMClosureObj *c = malloc(sizeof(VMClosureObj));
        c->function_index = fn_idx;
        c->capture_count = cap_count;
        c->captured = malloc(sizeof(VMValue) * (cap_count > 0 ? cap_count : 1));
        for (int i = 0; i < cap_count; i++) c->captured[i] = frame->slots[i];
        *stack_top++ = vm_closure_val(c);
        DISPATCH();
    }
    do_TRY_PUSH: {
        uint16_t catch_offset = (uint16_t)((frame->ip[0] << 8) | frame->ip[1]);
        frame->ip += 2;
        if (try_depth >= MAX_TRY_DEPTH) { fprintf(stderr, "too many nested try blocks\n"); exit(1); }
        TryHandler *h = &try_handlers[try_depth];
        h->saved_frame_count = frame_count;
        h->saved_stack_top = stack_top;
        h->target_frame = frame;
        h->target_ip = frame->ip + catch_offset;
        try_depth++;
        if (setjmp(h->jmp) == 0) {
            DISPATCH();
        } else {
            try_depth--;
            frame_count = h->saved_frame_count;
            stack_top = h->saved_stack_top;
            frame = h->target_frame;
            frame->ip = h->target_ip;
            if (has_raised_value) {
                *stack_top++ = raised_value;
                has_raised_value = 0;
            } else {
                char *msg_copy = malloc(strlen(error_message) + 1);
                strcpy(msg_copy, error_message);
                *stack_top++ = vm_str(msg_copy);
            }
            DISPATCH();
        }
    }
    do_TRY_POP: {
        try_depth--;
        DISPATCH();
    }
    do_RAISE: {
        VMValue msg = *--stack_top;
        vm_raise_value(msg); // noreturn -- longjmps to a handler, or exits
    }
    // Bitwise ops truncate to int64 (matching kiln's cvttsd2si approach)
    // then convert back -- same "assume NUMBER" scope limit as -, *, /,
    // and the comparison operators.
    do_BAND: { VMValue b = *--stack_top; VMValue *a = stack_top - 1; *a = vm_num((double)((int64_t)require_num(*a, "'&'") & (int64_t)require_num(b, "'&'"))); DISPATCH(); }
    do_BOR:  { VMValue b = *--stack_top; VMValue *a = stack_top - 1; *a = vm_num((double)((int64_t)require_num(*a, "'|'") | (int64_t)require_num(b, "'|'"))); DISPATCH(); }
    do_BXOR: { VMValue b = *--stack_top; VMValue *a = stack_top - 1; *a = vm_num((double)((int64_t)require_num(*a, "'^'") ^ (int64_t)require_num(b, "'^'"))); DISPATCH(); }
    do_BNOT: { stack_top[-1] = vm_num((double)(~(int64_t)require_num(stack_top[-1], "'~'"))); DISPATCH(); }
    do_SHL:  { VMValue b = *--stack_top; VMValue *a = stack_top - 1; *a = vm_num((double)((int64_t)require_num(*a, "'<<'") << (int64_t)require_num(b, "'<<'"))); DISPATCH(); }
    do_SHR:  { VMValue b = *--stack_top; VMValue *a = stack_top - 1; *a = vm_num((double)((int64_t)require_num(*a, "'>>'") >> (int64_t)require_num(b, "'>>'"))); DISPATCH(); }
    // Resets the stack to exactly `frame->slots + n` -- an absolute reset,
    // not N pops. A re-executed block (loop body, given/otherwise branch)
    // may declare a variable number of fresh locals depending on which
    // internal path it took (early `next`, nested given, ...), so "pop
    // however many were declared on a typical pass" can under- or
    // over-pop. Since every local is a stack slot and slot layout is
    // static per chunk, "what the stack should look like once this block
    // is done" is just "n slots past frame->slots" -- always correct
    // regardless of the path taken through the block this time.
    do_TRUNC_LOCALS: { uint8_t n = *frame->ip++; stack_top = frame->slots + n; DISPATCH(); }
}
