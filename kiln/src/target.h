#ifndef KILN_TARGET_H
#define KILN_TARGET_H

// Compile-time target selector (Plan A, phase A1). Nothing reads this yet
// past main.c storing it -- codegen/writer forks land in later phases.
// Global, not threaded as a parameter: every codegen/writer call site would
// otherwise need a target argument added in this phase just to ignore it,
// which is exactly the kind of premature plumbing the phased plan is
// avoiding until a phase actually needs to branch on it.
typedef enum {
    KILN_TARGET_LINUX,
    KILN_TARGET_WINDOWS
} KilnTarget;

void kiln_set_target(KilnTarget target);
KilnTarget kiln_get_target(void);

// Plan B: whether the runtime (heap/bytes/string_alloc/print/errors-raise)
// is emitted inline into this binary (KILN_LINK_STATIC, the default and
// the only mode before Plan B) or left external, calling into
// libkilnrt.so via real dynamic linking (KILN_LINK_SHARED, Linux only --
// Windows DLL equivalent is a later phase). Same "global, not threaded as
// a parameter" reasoning as KilnTarget above.
typedef enum {
    KILN_LINK_STATIC,
    KILN_LINK_SHARED
} KilnLinkMode;

void kiln_set_link_mode(KilnLinkMode mode);
KilnLinkMode kiln_get_link_mode(void);

// Separate from KilnLinkMode on purpose: link mode answers "should THIS
// routine's own call sites (heap_emit_alloc etc.) reach their callee via
// a local call or a GOT call" -- but codegen/layout.c's globals-address
// selection needs to answer a DIFFERENT question, "which executable
// layout owns the globals this code will read at runtime", and that
// answer is the shared-mode layout in BOTH cases: when compiling a
// normal --link=shared executable AND when compiling the runtime that
// goes into libkilnrt.so (its heap_alloc etc. always run on behalf of
// some shared-mode executable's globals, never its own). Conflating
// these into one flag was a real bug caught by testing (see
// elf/elf_so_writer.c's comment) -- kiln_set_link_mode(STATIC) to get
// local intra-.so calls also silently broke the globals address.
void kiln_set_compiling_so(int flag);
int kiln_is_compiling_so(void);

#endif
