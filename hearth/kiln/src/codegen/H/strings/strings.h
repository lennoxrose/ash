#ifndef KILN_STRINGS_H
#define KILN_STRINGS_H

// primary()'s TOKEN_STRING handling: embeds the literal's escaped bytes
// into the code buffer (jumped over, so normal control flow never tries
// to execute them as instructions -- same technique fn declarations use
// for their bodies), pushes (tag=STRING, payload=<fixed absolute
// address>). Kiln generates a fixed, non-PIE executable, so a literal's
// address is knowable at compile time -- no heap needed for these.
void codegen_string_literal(void);

// Same emission as codegen_string_literal, but takes raw escaped bytes
// directly instead of reading the parser's global `current` token --
// used by constants.c to fold an imported string `let` at each use site,
// where `current` is the identifier/dot chain, not a string token.
void codegen_string_literal_bytes(const char *text, int len);

// Both assume operands already popped with a_payload in RAX, b_payload
// in RCX (a_tag/b_tag, if the caller still needs them, must be read and
// saved BEFORE calling -- these clobber RAX,RBX,RCX,RDX,RSI,RDI freely).

// Pushes (tag=STRING, payload=<new heap address>) -- the concatenation.
// Needs a real heap allocation (see codegen/H/runtime/heap.h) since the combined
// size is only known at runtime.
void codegen_string_concat(void);

// Pushes (tag=NUMBER, payload=1.0/0.0) -- byte-equal (invert=0) or
// byte-different (invert=1, i.e. `!=`).
void codegen_string_compare(int invert);

// Lexicographic (bytewise) order of two strings, a_payload=RAX, b_payload=RCX.
// Pushes nothing: leaves the flags as if a cmp of a against b had run, so the
// caller can branch with the same unsigned conditions ucomisd uses (COND_B/BE/
// A/AE). Clobbers RAX, RCX, RBX, RDX, RSI, RDI.
void codegen_string_order(void);

// Assumes index already pushed (top) and string already pushed (below
// it), both (tag, payload) pairs -- postfix()'s `s[i]` read, the
// TAG_STRING case (previously missing entirely -- see missing.md #1:
// falling through to the array path here would have read the string's
// own content bytes as if they were an array-object struct). Pops both,
// bounds-checks, pushes a fresh 1-char string. Read-only: strings are
// immutable, `s[i] = x` is a parse-time-caught statement shape kiln's
// grammar doesn't even offer a codegen path for (see parser.c).
void codegen_string_index_read(void);

#endif
