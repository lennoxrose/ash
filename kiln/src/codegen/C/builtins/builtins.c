#include <string.h>
#include "codegen/H/builtins/builtins.h"
#include "codegen/H/expressions/expr.h"
#include "codegen/H/collections/arrays.h"
#include "codegen/H/collections/maps.h"
#include "codegen/H/collections/matrix_mul.h"
#include "codegen/H/builtins/math_builtins.h"
#include "codegen/H/builtins/convert_builtins.h"
#include "codegen/H/builtins/higher_order.h"
#include "codegen/H/collections/keys_values.h"
#include "codegen/H/strings/string_builtins.h"
#include "codegen/H/io/file_builtins.h"
#include "codegen/H/io/input_builtin.h"
#include "codegen/H/io/argv_builtin.h"
#include "codegen/H/builtins/type_builtin.h"
#include "codegen/H/builtins/exit_builtin.h"
#include "parser/H/core/parser.h"

int codegen_try_builtin_call(const char *name, int len) {
    if (len == 4 && strncmp(name, "argv", 4) == 0) {
        expect(TOKEN_RPAREN, "expected ')' after 'argv('");
        codegen_builtin_argv();
        return 1;
    }
    if (len == 5 && strncmp(name, "input", 5) == 0) {
        expect(TOKEN_RPAREN, "expected ')' after 'input('");
        codegen_builtin_input();
        return 1;
    }
    if (len == 9 && strncmp(name, "read_file", 9) == 0) {
        codegen_expression();
        expect(TOKEN_RPAREN, "expected ')' after argument");
        codegen_builtin_read_file();
        return 1;
    }
    if (len == 10 && strncmp(name, "write_file", 10) == 0) {
        codegen_builtin_write_file();
        return 1;
    }
    if (len == 11 && strncmp(name, "append_file", 11) == 0) {
        codegen_builtin_append_file();
        return 1;
    }
    if (len == 11 && strncmp(name, "rename_file", 11) == 0) {
        codegen_builtin_rename_file();
        return 1;
    }
    if (len == 11 && strncmp(name, "delete_file", 11) == 0) {
        codegen_builtin_delete_file();
        return 1;
    }
    if (len == 8 && strncmp(name, "make_dir", 8) == 0) {
        codegen_builtin_make_dir();
        return 1;
    }
    if (len == 8 && strncmp(name, "list_dir", 8) == 0) {
        codegen_builtin_list_dir();
        return 1;
    }
    if (len == 11 && strncmp(name, "file_exists", 11) == 0) {
        codegen_expression();
        expect(TOKEN_RPAREN, "expected ')' after argument");
        codegen_builtin_file_exists();
        return 1;
    }
    if (len == 5 && strncmp(name, "upper", 5) == 0) {
        codegen_expression();
        expect(TOKEN_RPAREN, "expected ')' after argument");
        codegen_builtin_upper();
        return 1;
    }
    if (len == 5 && strncmp(name, "lower", 5) == 0) {
        codegen_expression();
        expect(TOKEN_RPAREN, "expected ')' after argument");
        codegen_builtin_lower();
        return 1;
    }
    if (len == 4 && strncmp(name, "trim", 4) == 0) {
        codegen_expression();
        expect(TOKEN_RPAREN, "expected ')' after argument");
        codegen_builtin_trim();
        return 1;
    }
    if (len == 9 && strncmp(name, "substring", 9) == 0) {
        codegen_builtin_substring();
        return 1;
    }
    if (len == 7 && strncmp(name, "indexOf", 7) == 0) {
        codegen_builtin_indexof();
        return 1;
    }
    if (len == 5 && strncmp(name, "split", 5) == 0) {
        codegen_builtin_split();
        return 1;
    }
    if (len == 4 && strncmp(name, "join", 4) == 0) {
        codegen_builtin_join();
        return 1;
    }
    if (len == 7 && strncmp(name, "replace", 7) == 0) {
        codegen_builtin_replace();
        return 1;
    }
    if (len == 4 && strncmp(name, "keys", 4) == 0) {
        codegen_expression();
        expect(TOKEN_RPAREN, "expected ')' after argument");
        codegen_builtin_keys();
        return 1;
    }
    if (len == 6 && strncmp(name, "values", 6) == 0) {
        codegen_expression();
        expect(TOKEN_RPAREN, "expected ')' after argument");
        codegen_builtin_values();
        return 1;
    }
    if (len == 8 && strncmp(name, "contains", 8) == 0) { codegen_builtin_contains(); return 1; }
    if (len == 11 && strncmp(name, "starts_with", 11) == 0) { codegen_builtin_starts_with(); return 1; }
    if (len == 9 && strncmp(name, "ends_with", 9) == 0) { codegen_builtin_ends_with(); return 1; }
    if (len == 6 && strncmp(name, "repeat", 6) == 0) { codegen_builtin_repeat(); return 1; }
    if (len == 4 && strncmp(name, "exit", 4) == 0) { codegen_builtin_exit(); return 1; }
    if (len == 4 && strncmp(name, "sort", 4) == 0) {
        codegen_builtin_sort();
        return 1;
    }
    if (len == 3 && strncmp(name, "map", 3) == 0) {
        codegen_builtin_map();
        return 1;
    }
    if (len == 6 && strncmp(name, "filter", 6) == 0) {
        codegen_builtin_filter();
        return 1;
    }
    if (len == 6 && strncmp(name, "reduce", 6) == 0) {
        codegen_builtin_reduce();
        return 1;
    }
    if (len == 3 && strncmp(name, "str", 3) == 0) {
        codegen_expression();
        expect(TOKEN_RPAREN, "expected ')' after argument");
        codegen_builtin_str();
        return 1;
    }
    if (len == 3 && strncmp(name, "num", 3) == 0) {
        codegen_expression();
        expect(TOKEN_RPAREN, "expected ')' after argument");
        codegen_builtin_num();
        return 1;
    }
    if (len == 4 && strncmp(name, "sqrt", 4) == 0) {
        codegen_expression();
        expect(TOKEN_RPAREN, "expected ')' after argument");
        codegen_builtin_sqrt();
        return 1;
    }
    if (len == 3 && strncmp(name, "abs", 3) == 0) {
        codegen_expression();
        expect(TOKEN_RPAREN, "expected ')' after argument");
        codegen_builtin_abs();
        return 1;
    }
    if (len == 5 && strncmp(name, "floor", 5) == 0) {
        codegen_expression();
        expect(TOKEN_RPAREN, "expected ')' after argument");
        codegen_builtin_floor();
        return 1;
    }
    if (len == 4 && strncmp(name, "push", 4) == 0) {
        codegen_expression();
        expect(TOKEN_COMMA, "expected ',' after array argument");
        codegen_expression();
        expect(TOKEN_RPAREN, "expected ')' after arguments");
        codegen_builtin_push();
        return 1;
    }
    if (len == 10 && strncmp(name, "matrix_mul", 10) == 0) {
        codegen_expression();
        expect(TOKEN_COMMA, "expected ',' after array argument");
        codegen_expression();
        expect(TOKEN_RPAREN, "expected ')' after arguments");
        codegen_builtin_matrix_mul();
        return 1;
    }
    if (len == 3 && strncmp(name, "len", 3) == 0) {
        codegen_expression();
        expect(TOKEN_RPAREN, "expected ')' after argument");
        codegen_builtin_len();
        return 1;
    }
    if (len == 3 && strncmp(name, "has", 3) == 0) {
        codegen_expression();
        expect(TOKEN_COMMA, "expected ',' after map argument");
        codegen_expression();
        expect(TOKEN_RPAREN, "expected ')' after arguments");
        codegen_builtin_has();
        return 1;
    }
    if (len == 6 && strncmp(name, "delete", 6) == 0) {
        codegen_expression();
        expect(TOKEN_COMMA, "expected ',' after map argument");
        codegen_expression();
        expect(TOKEN_RPAREN, "expected ')' after arguments");
        codegen_builtin_delete();
        return 1;
    }
    if (len == 3 && strncmp(name, "pop", 3) == 0) {
        codegen_expression();
        expect(TOKEN_RPAREN, "expected ')' after argument");
        codegen_builtin_pop();
        return 1;
    }
    if (len == 6 && strncmp(name, "insert", 6) == 0) {
        codegen_expression();
        expect(TOKEN_COMMA, "expected ',' after array argument");
        codegen_expression();
        expect(TOKEN_COMMA, "expected ',' after index argument");
        codegen_expression();
        expect(TOKEN_RPAREN, "expected ')' after arguments");
        codegen_builtin_insert();
        return 1;
    }
    if (len == 5 && strncmp(name, "slice", 5) == 0) {
        codegen_expression();
        expect(TOKEN_COMMA, "expected ',' after array argument");
        codegen_expression();
        expect(TOKEN_COMMA, "expected ',' after start argument");
        codegen_expression();
        expect(TOKEN_RPAREN, "expected ')' after arguments");
        codegen_builtin_slice();
        return 1;
    }
    if (len == 3 && strncmp(name, "chr", 3) == 0) {
        codegen_expression();
        expect(TOKEN_RPAREN, "expected ')' after argument");
        codegen_builtin_chr();
        return 1;
    }
    if (len == 3 && strncmp(name, "ord", 3) == 0) {
        codegen_expression();
        expect(TOKEN_RPAREN, "expected ')' after argument");
        codegen_builtin_ord();
        return 1;
    }
    if (len == 4 && strncmp(name, "type", 4) == 0) {
        codegen_expression();
        expect(TOKEN_RPAREN, "expected ')' after argument");
        codegen_builtin_type();
        return 1;
    }
    return 0;
}

// Argument count each builtin is wrapped with when used as a value
// (`map(xs, str)`); -1 if `name` is not a builtin. Same set and arities as
// ashvm's builtin table.
int codegen_builtin_arity(const char *name, int len) {
    static const struct { const char *name; int arity; } table[] = {
        {"len", 1}, {"push", 2}, {"input", 0}, {"str", 1}, {"num", 1}, {"sqrt", 1}, {"abs", 1}, {"floor", 1},
        {"map", 2}, {"filter", 2}, {"reduce", 3}, {"keys", 1}, {"values", 1}, {"has", 2}, {"delete", 2},
        {"split", 2}, {"join", 2}, {"substring", 3}, {"indexOf", 2}, {"replace", 3}, {"upper", 1}, {"lower", 1}, {"trim", 1},
        {"read_file", 1}, {"write_file", 2}, {"append_file", 2}, {"file_exists", 1},
        {"type", 1}, {"chr", 1}, {"ord", 1}, {"pop", 1}, {"insert", 3}, {"slice", 3}, {"sort", 1},
        {"rename_file", 2}, {"delete_file", 1}, {"make_dir", 1}, {"list_dir", 1},
        {"contains", 2}, {"starts_with", 2}, {"ends_with", 2}, {"repeat", 2}, {"exit", 1}, {"argv", 0},
    };
    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); i++) {
        if ((int)strlen(table[i].name) == len && strncmp(table[i].name, name, (size_t)len) == 0) return table[i].arity;
    }
    return -1;
}
