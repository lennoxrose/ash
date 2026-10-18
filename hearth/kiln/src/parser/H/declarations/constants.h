#ifndef KILN_CONSTANTS_H
#define KILN_CONSTANTS_H

#define MAX_KILN_CONSTS 32

typedef enum { KILN_CONST_NUMBER, KILN_CONST_STRING } KilnConstantKind;

typedef struct {
    char name[96]; // namespaced, e.g. "math.PI" -- longer than KilnFunction's
                    // 64 since it also has to fit a namespace prefix on top
                    // of a normal identifier
    KilnConstantKind kind;
    double num_value;   // valid when kind == KILN_CONST_NUMBER
    char *str_bytes;    // valid when kind == KILN_CONST_STRING -- raw
                          // (still-escaped) bytes from the source token,
                          // NOT yet processed; codegen_constant_value
                          // re-runs escape processing at each use site via
                          // codegen_string_literal_bytes, same as a normal
                          // string literal would
    int str_len;
} KilnConstant;

// Declares NAME (already namespaced by the caller, e.g. "math.PI") as a
// number constant. Calls parse_error() on table-full or name-too-long.
KilnConstant *declare_number_constant(const char *name, int len, double value);

// Same, for a string constant. str_bytes/str_len are the RAW (unescaped)
// source token bytes -- constants.c does not own or copy this pointer
// beyond storing it; the caller (parser.c's local_statement) must pass
// bytes from a buffer that outlives the whole compile (the imported
// file's source buffer, which imports.c already never frees -- see
// imports.c's read_import_file comment).
KilnConstant *declare_string_constant(const char *name, int len, const char *str_bytes, int str_len);

KilnConstant *resolve_constant(const char *name, int len);

// Emits the same codegen a literal of this constant's value would have
// emitted inline (a NUMBER push or a STRING alloc+push) -- used at every
// `ns.CONST` use site.
void codegen_constant_value(KilnConstant *k);

#endif
