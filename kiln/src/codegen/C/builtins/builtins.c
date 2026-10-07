#include <string.h>
#include "codegen/H/builtins/builtins.h"
#include "codegen/H/expressions/expr.h"
#include "codegen/H/collections/arrays.h"
#include "codegen/H/collections/maps.h"
#include "codegen/H/builtins/math_builtins.h"
#include "codegen/H/builtins/convert_builtins.h"
#include "codegen/H/builtins/higher_order.h"
#include "codegen/H/collections/keys_values.h"
#include "codegen/H/strings/string_builtins.h"
#include "codegen/H/io/file_builtins.h"
#include "codegen/H/io/input_builtin.h"
#include "codegen/H/io/argv_builtin.h"
#include "codegen/H/builtins/type_builtin.h"
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
    if (len == 4 && strncmp(name, "type", 4) == 0) {
        codegen_expression();
        expect(TOKEN_RPAREN, "expected ')' after argument");
        codegen_builtin_type();
        return 1;
    }
    return 0;
}
