#ifndef JRANGE_H
#define JRANGE_H

/* Value-range checks for the ownership pass (`memory-safety: on`): the Rust-side interval state in
 * mnemo.a (flow.h, "value ranges") is fed with what is known about int variables and array lengths,
 * and an operation that can only fail is a compile error, like Rust's deny-by-default lints:
 *   - a[i] where i is always negative or always past the length of a
 *   - an int +, -, *, ++, --, +=, -=, *= whose result is always outside the 32-bit range (not under --wrap-int)
 *   - x / y or x % y where y is always 0 (a literal divisor stays a run-time exception)
 *   - new T[n] where n is always negative
 * Only certain failures are reported ("always"): a value that merely might be out of range is left to
 * the run-time check. Unknown values (parameters, fields, calls) are never reported. Inside a try block
 * with a catch the checks stay quiet: the program may be provoking the exception on purpose. */

#include "mnemo/H/flow/flow.h"
#include "jparse/H/jast.h"
#include "jtypes/H/jtypes.h"

typedef struct {
    MnemoWalk *m;
    int mute;          /* > 0: a loop pass that is not final, or inside a try with catch: report nothing */
    JList reported;    /* Node* already diagnosed */
} RgCtx;

void rg_declare(RgCtx *r, Node *local);                 /* N_LOCAL after its initializer was walked */
void rg_forget(RgCtx *r, JLocal *l);                    /* the variable gets an unknown value */
void rg_assign(RgCtx *r, Node *assign);                 /* N_ASSIGN after both sides were walked */
void rg_incdec(RgCtx *r, Node *n);                      /* N_UNARY ++/-- and N_POSTFIX after the operand was walked */
void rg_check_index(RgCtx *r, Node *index);             /* N_INDEX after both sides were walked */
void rg_check_binary(RgCtx *r, Node *bin);              /* N_BINARY after both sides were walked */
void rg_check_newarray(RgCtx *r, Node *na);             /* N_NEWARRAY after its dimensions were walked */
/* The condition `cond` evaluated to `truth`: narrow the state; the path dies when that is impossible. */
void rg_assume(RgCtx *r, Node *cond, int truth);
void rg_forget_assigned(RgCtx *r, Node *block);         /* forget every local the block assigns (entry of a catch) */
/* A method whose body calls itself before any branch, loop or return can never finish. */
void rg_check_recursion(JMethod *m);

#endif
