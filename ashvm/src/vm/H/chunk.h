#ifndef ASH_VM_CHUNK_H
#define ASH_VM_CHUNK_H
#include <stdint.h>
#include "value/H/value.h"

typedef enum {
    OP_CONST,
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD, OP_NEG,
    OP_NOT, OP_EQ, OP_NEQ, OP_LT, OP_LE, OP_GT, OP_GE,
    OP_AND, OP_OR,
    OP_PRINT, OP_POP,
    OP_GET_LOCAL, OP_SET_LOCAL,
    OP_JUMP, OP_JUMP_IF_FALSE, OP_LOOP,
    OP_CALL, OP_RETURN,
    OP_ACC_LOCAL,
    OP_CMP_JUMP,
    OP_ARRAY,
    OP_INDEX_GET,
    OP_INDEX_SET,
    OP_MAP,
    OP_CALL_VALUE,
    OP_CALL_BUILTIN,
    OP_MAKE_CLOSURE,
    OP_TRY_PUSH,
    OP_TRY_POP
} OpCode;

typedef struct {
    int count, capacity;
    uint8_t *code;
    const char **pos;   // parallel to code; populated only at opcode-start bytes
    int *poslen;
    int const_count, const_capacity;
    VMValue *constants;
} Chunk;

void chunk_init(Chunk *c);
int chunk_write(Chunk *c, uint8_t byte);
int chunk_write_pos(Chunk *c, uint8_t byte, const char *pos, int len);
int chunk_add_constant(Chunk *c, VMValue value);

#endif
