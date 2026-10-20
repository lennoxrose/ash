#include <math.h>
#include <stdlib.h>
#include <string.h>
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

// Set by the compiler: called when a function needs a 257th constant (code names a constant with one byte).
void (*chunk_constant_overflow)(void) = NULL;

static int constant_equal(VMValue a, VMValue b) {
    if (a.type != b.type) return 0;
    switch (a.type) {
        case VM_NUM: case VM_BOOL: return a.number == b.number && signbit(a.number) == signbit(b.number);
        case VM_STR: return a.str && b.str && strcmp(a.str, b.str) == 0;
        case VM_NIL: return 1;
        default: return 0;
    }
}

// Every use of a constant adds an entry, and the table grows by doubling. The growth path is also where a big
// function is dealt with, at no cost to the others: once a function has 128 constants the table grows one entry at
// a time, so every further constant passes through here, finds an equal one already in the table (and reuses it), or,
// past 256, is an error (the index used to wrap round, and the code read another constant).
static __attribute__((noinline)) int chunk_constant_slow(Chunk *c, VMValue value) {
    if (c->const_count >= 128) {
        for (int i = 0; i < c->const_count; i++) if (constant_equal(c->constants[i], value)) return i;
        if (c->const_count > 255 && chunk_constant_overflow) chunk_constant_overflow();
        c->const_capacity = c->const_count + 1;
    } else {
        c->const_capacity = c->const_capacity == 0 ? 16 : c->const_capacity * 2;
    }
    c->constants = realloc(c->constants, c->const_capacity * sizeof(VMValue));
    c->constants[c->const_count] = value;
    return c->const_count++;
}

int chunk_add_constant(Chunk *c, VMValue value) {
    if (c->const_count >= c->const_capacity) return chunk_constant_slow(c, value);
    c->constants[c->const_count] = value;
    return c->const_count++;
}
