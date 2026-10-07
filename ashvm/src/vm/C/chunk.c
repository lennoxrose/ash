#include <stdlib.h>
#include "vm/H/chunk.h"

void chunk_init(Chunk *c) {
    c->count = 0; c->capacity = 0; c->code = NULL;
    c->pos = NULL; c->poslen = NULL;
    c->const_count = 0; c->const_capacity = 0; c->constants = NULL;
}

static void chunk_grow(Chunk *c) {
    if (c->count >= c->capacity) {
        c->capacity = c->capacity == 0 ? 64 : c->capacity * 2;
        c->code = realloc(c->code, c->capacity);
        c->pos = realloc(c->pos, c->capacity * sizeof(const char *));
        c->poslen = realloc(c->poslen, c->capacity * sizeof(int));
    }
}

int chunk_write(Chunk *c, uint8_t byte) {
    chunk_grow(c);
    c->code[c->count] = byte;
    c->pos[c->count] = NULL;
    c->poslen[c->count] = 0;
    return c->count++;
}

int chunk_write_pos(Chunk *c, uint8_t byte, const char *pos, int len) {
    chunk_grow(c);
    c->code[c->count] = byte;
    c->pos[c->count] = pos;
    c->poslen[c->count] = len;
    return c->count++;
}

int chunk_add_constant(Chunk *c, VMValue value) {
    if (c->const_count >= c->const_capacity) {
        c->const_capacity = c->const_capacity == 0 ? 16 : c->const_capacity * 2;
        c->constants = realloc(c->constants, c->const_capacity * sizeof(VMValue));
    }
    c->constants[c->const_count] = value;
    return c->const_count++;
}
