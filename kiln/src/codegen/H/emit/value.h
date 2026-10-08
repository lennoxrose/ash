#ifndef KILN_VALUE_H
#define KILN_VALUE_H

// Milestone 5: an ash value is no longer always a bare double -- it can
// also be a string. Every value is now a (tag, payload) pair, 16 bytes
// total, moved around generically via GP registers (raw bit-copying
// doesn't care what the bits mean -- see codegen/C/expressions/expr.c's header
// comment). XMM registers only ever hold the payload transiently, at the
// point of actually doing floating-point arithmetic on a NUMBER-tagged
// value.
//
// A string's payload is an absolute address (kiln generates a fixed,
// non-PIE executable, so absolute addresses are stable and known at
// compile time) pointing at a length-prefixed byte block: 8 bytes of
// length, followed by that many raw bytes. String literals are embedded
// directly in the code buffer (see codegen/C/strings/strings.c); concatenation
// results are heap-allocated (see codegen/C/runtime/heap.c) since their size is
// only known at runtime.
#define TAG_NUMBER 0
#define TAG_STRING 1
#define TAG_ARRAY 2
#define TAG_MAP 3
#define TAG_FUNCTION 4

// A nil value's payload is always exactly 0 -- deliberately, so it
// coincides with +0.0's bit pattern. That's what lets nil ride the
// EXISTING truthy-test and numeric-comparison codegen (both just
// ucomisd the payload against 0.0) without needing its own special case
// anywhere except print (see codegen/C/runtime/print_int.c's dispatch) and the
// tag-equality short-circuit in comparisons (already tag-aware since
// M5's `==`/`!=`, see codegen/C/expressions/expr_bool.c -- nil==nil compares equal
// tags then falls into the numeric path, 0.0==0.0; nil compared against
// any other tag is caught by the tags-differ branch before ever
// touching the payload).
#define TAG_NIL 5

// A real boolean: `yes` / `no`, and what comparisons, `!`, `and`/`or`, has() and
// the *_exists / contains-style builtins produce. The payload is 1.0 / 0.0, so
// truthiness and arithmetic (which only look at the payload) keep working;
// `yes == 1` is false because the tags differ. Prints as yes / no.
#define TAG_BOOL 6

// An array's payload is the address of a small, fixed-size, STABLE
// "array object" (see codegen/H/strings/strings.h's comment on why absolute
// addresses are safe: kiln is always a fixed, non-PIE executable):
//   [0]  capacity (int64, how many elements the data block has room for)
//   [8]  count (int64, how many are actually in use)
//   [16] data_ptr (address of a SEPARATE elements block)
// The elements block itself ([16*i]=tag, [16*i+8]=payload, for i in
// [0,capacity)) is what actually moves on growth -- a fresh, bigger
// block gets allocated and the old one's contents copied over (the bump
// allocator can't grow in place), but only data_ptr's VALUE changes.
// The array object's own address -- what every variable/slot holding
// this array actually stores -- never moves. This matters: without it,
// `push(arr, x)` growing the array would silently leave every existing
// reference to `arr` pointing at stale, abandoned memory. (ashvm gets
// this for free because VMArray is a pointer to a struct whose `items`
// field can be realloc'd independently of the struct's own address --
// this is kiln's equivalent of that same shape.)
#define ARRAY_OBJECT_SIZE 24

// A map uses the exact same stable-object-plus-resizable-data shape as
// an array (see above -- same reason: growth must not move the address
// every existing reference to the map already holds):
//   [0]  capacity   [8]  count   [16] entries_ptr
// Each entry is 32 bytes: [0]=key tag (always TAG_STRING) [8]=key
// payload [16]=value tag [24]=value payload. Lookup is a linear scan,
// not a hash table -- ashvm uses a real hash map; this is a documented
// simplification (O(n) instead of O(1)) for implementation reach, not a
// semantic difference a program could observe other than speed.
#define MAP_OBJECT_SIZE 24
#define MAP_ENTRY_SIZE 32

// A function/closure's payload is the address of a small, fixed (NEVER
// resized -- closures are immutable once created, no growth concern like
// arrays/maps) heap block:
//   [0]  absolute code address (already resolved to KILN_LOAD_BASE +
//        KILN_CODE_START_OFFSET + code_offset at compile time -- kiln is
//        a fixed non-PIE executable, see elf/H/elf_writer.h, so this is
//        known and stable)
//   [8]  capture_count (int64)
//   [16+16*i] capture i's tag   [16+16*i+8] capture i's payload
// A plain named function used as a value (not immediately called) gets
// capture_count=0 -- the same shape as any closure with zero captures, so
// codegen/C/functions/closures.c's indirect-call codegen never special-cases "0
// captures", it just runs a 0-iteration loop.
//
// Scope limit (documented, not silently pretended away -- same spirit as
// the map's linear-scan note above): captures are snapshotted BY VALUE at
// closure-creation time, not by reference to a shared mutable cell. This
// supports closures that read outer variables (partial application, e.g.
// `fn make_adder(n) { return fn(x) { return x + n; }; }`) but NOT closures
// that mutate a captured variable and expect that mutation visible on the
// next call (the classic mutable "counter" idiom) -- that needs a real
// upvalue mechanism (boxed/heap-indirect variables), which conflicts with
// kiln's single-pass, fixed-stack-slot variable model and is out of scope
// for this milestone.
#define FUNCTION_OBJECT_HEADER_SIZE 16

#endif
