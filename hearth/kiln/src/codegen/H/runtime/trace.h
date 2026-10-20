#ifndef KILN_TRACE_H
#define KILN_TRACE_H
#include "codegen/H/emit/emit.h"

// Stack traces of an uncaught exception (static builds only). The IR carries `//@<line> [<name>]` marker comments
// before the statements of project functions (jlower); the parser reports each one here with the code offset it
// starts at. At the end of the compile the table (code address -> line, function name) is appended to the code.
// At run time the raise routine copies the return addresses of the rbp chain into a fixed area when the raise
// reaches the top-level handler (nothing could catch it before), and the builtins trace_n() / trace_fn(i) /
// trace_line(i) read that area and the table. Nothing is emitted for a statement, and no instruction runs, on any
// path that does not raise.

// Compile time ---------------------------------------------------------------------------------------------
void trace_reset(void);
void trace_note(int code_offset, int line, const char *name, int name_len);   // a statement starts here (name: first marker of a function, else NULL)
void trace_note_end(int code_offset);                                          // a function ends here
void trace_finish(CodeBuf *code);                                              // appends the table and patches the builtins' references

// Code generation ------------------------------------------------------------------------------------------
void trace_emit_main_frame(CodeBuf *code);        // program start: remember the top-level rbp
void trace_emit_capture(CodeBuf *code);           // inside the raise routine: RAX = try depth, RDI = address of the raise site
void codegen_builtin_trace_n(void);               // trace_n()
void codegen_builtin_trace_fn(void);              // trace_fn(i): "Class.method(File.ash" or "" for a frame without one
void codegen_builtin_trace_line(void);            // trace_line(i): the line, 0 without one

// Reads the marker comment right before the token at `tok_start` (see ir_lex.h); returns 1 and fills the fields.
#endif
