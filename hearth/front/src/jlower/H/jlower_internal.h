#ifndef JLOWER_INTERNAL_H
#define JLOWER_INTERNAL_H

#include "jlower/H/jlower.h"
#include "jtypes/H/check.h"

/* State of the statement currently being lowered. */
typedef struct LoopCtx {
    const char *update; /* for-loop update statements, emitted before `next` */
    int is_switch;      /* a switch lowered to an if-chain: only a trailing break is allowed */
    int wrapper;        /* a switch lowered as `during yes {...; stop;}`: break is `stop`, continue leaves via cont_flag */
    char *cont_flag;    /* set before stop when `continue` crosses the wrapper */
    int cont_used;
    int fin_mark;       /* L.fin_n when the loop began: break/continue run the finally bodies above it */
    int drop_mark;      /* L.drop_n when the loop began: break/continue free the droppables above it */
    struct LoopCtx *up;
} LoopCtx;

typedef struct {
    JBuf pre;       /* hoisted statements for the statement being emitted */
    int tmp;        /* temp counter */
    JClass *cls;    /* class being lowered */
    JMethod *method;
    int is_static;
    LoopCtx *loop;
    Node *fin[16];  /* enclosing finally bodies, innermost last (inlined before each return) */
    int fin_n;
    JLocal *drops[64];  /* live sole-owner locals (see JLocal.drop), innermost last */
    int drop_n;
    int drop_base;      /* entries below belong to an enclosing function; a lambda's return must not free them */
} LState;
extern LState L;
int jl_drop_ok(JType *t);
void jl_emit_drop(JBuf *b, int ind, JType *t, const char *var, int elems); /* frees `var`, an owned local of type t, the fields its object owns, and its elements when `elems` */
const char *jl_drop_fn(JType *t);   /* builtin that frees a local of this type: mem_free_arr arrays, mem_free_obj objects, mem_free Strings */   /* the engine can free a sole-owner local of this type */
extern int jl_wrap_int; /* --wrap-int: Java 32-bit int overflow (costs about 2x on arithmetic-heavy loops) */

/* lower_expr.c */
char *lx(Node *e);                                  /* expression -> old-Ash expression */
char *str_of(JType *t, const char *text);           /* string conversion by static type */
char *quote_str(const char *s);
char *sname(const char *n);                         /* identifier safe in old Ash */
char *fn_name(JMethod *m, const char *kind);        /* e.g. demo_Main__main_3 */
char *method_key(JMethod *m);                       /* vtable key */
char *cls_state(JClass *c);                         /* C__S */
char *cls_is(JClass *c);                            /* C__IS */
char *cls_vt(JClass *c);                            /* C__VT */
char *new_temp(void);
void mark_used(JClass *c);                          /* class needs emitting */
int used_count(void);
JClass *used_at(int i);
void lerr(Node *at, const char *fmt, ...);
int is_array_list(JType *t);                        /* List / ArrayList: represented by an Ash array */
int tmpl_uses_receiver(const char *tmpl);
char *expand_tmpl(const char *tmpl, const char *recv, char **args, int nargs);
char *lower_ctor_call(Node *n);                     /* N_CTORCALL statement text */
void alias_set(JLocal *l, const char *text);        /* pattern bindings read the scrutinee */
char *alias_get(JLocal *l);
int is_native_value_class(JClass *c);               /* represented by an Ash primitive/array/map */

/* lower_stmt.c */
void ls(JBuf *b, Node *s, int ind);
void lblock_body(JBuf *b, Node *block, int ind);
void emit_line(JBuf *b, int ind, const char *fmt, ...);
void flush_pre(JBuf *b, int ind);
char *lower_lambda_body(Node *lam, JMethod *sam, int ind);
char *lower_switch_expr(Node *n);
void emit_assign_stmt(JBuf *b, Node *e, int ind);

/* lower_prelude.c */
void emit_prelude(JBuf *b);
void emit_prelude_fmt(JBuf *b);                    /* String.format runtime (lower_prelude_fmt.c) */
void emit_prelude_long(JBuf *b);                   /* exact 64-bit long runtime (lower_prelude_long.c) */
char *prune_module(const char *module); /* dead-code elimination from start() (lower_prune.c) */
JMethod *find_ctor(JClass *c, int nparams);

#endif
