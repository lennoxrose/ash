#ifndef KILN_PARSER_INTERNAL_H
#define KILN_PARSER_INTERNAL_H

// Cross-file link between parser.c (state, simple statements, the
// statement() dispatcher) and parser_control.c (given/during/forge -- the
// statement kinds involved enough to warrant their own file), not part of
// the public API (parser.h).
void block(void);
void given_statement(void);
void during_statement(void);
void forge_statement(void);
void attempt_statement(void);
void each_statement(void);

// statement() itself (parser.c's dispatcher) also needs to be callable
// from imports.c's parse_imported_file_body(), which drives the same
// top-level "while not EOF, parse a statement" loop for a recursively
// parsed imported file -- so it's declared here rather than staying
// static to parser.c.
void statement(void);
void import_statement(void);

#endif
