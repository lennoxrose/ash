#ifndef JDRIVER_H
#define JDRIVER_H

/* The single entry point ashvm and kiln use to turn Ash source into something they can execute.
 *
 * The pipeline is: lex and parse (jlex, jparse) -> type-check (jtypes) -> lower (jlower). The result is
 * the internal IR (see ir/H/ir_lex.h): a unit of top-level `forge` and `local` declarations in which
 * `forge start()` is the program entry. The IR is never exposed to users; it is passed in memory from
 * this module to the ashvm compiler or the kiln code generator.
 *
 * Ownership: the returned text lives in the front end's arena and is never freed.
 * Errors: diagnostics go to stderr; a failed compilation returns NULL (or non-zero). */

/* Parses, checks and lowers the program whose entry file is `path`. `wrap_int` selects Java's 32-bit
 * int overflow (about 2x slower arithmetic loops). */
char *jdriver_compile_file(const char *path, int wrap_int);

/* Same for source text already in memory; `path` names it in diagnostics and anchors package roots. */
char *jdriver_compile_source(const char *path, const char *src, int wrap_int);

/* Parses and type-checks only. Returns 0 when the program is well-typed. */
int jdriver_check_file(const char *path);

#endif
