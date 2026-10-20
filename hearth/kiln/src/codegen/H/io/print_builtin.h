#ifndef KILN_PRINT_BUILTIN_H
#define KILN_PRINT_BUILTIN_H

// print_raw(s): writes the String s to standard output with no newline (System.out.print). It self-parses its
// argument, starting right after the already-consumed '('. Output is unbuffered, so print_raw("") is a no-op
// that System.out.flush() can use.
void codegen_builtin_print_raw(void);

#endif
