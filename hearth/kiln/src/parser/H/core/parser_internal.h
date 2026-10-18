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

void statement(void);

#endif
