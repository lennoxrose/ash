#ifndef CHECK_H
#define CHECK_H

#include "jtypes/H/jtypes.h"

/* Internal checker state shared by check_expr.c, check_stmt.c and check_class.c. */

struct JLocal {
    const char *name;
    JType *type;
    int flags;       /* M_FINAL */
    int nonnull;     /* flow-narrowed to non-null */
    int assigns;     /* assignments after declaration */
    int has_init;
    int is_param;
    int index;       /* position in Ctx.locals */
    Node *decl;
    Node *captured_at; /* first lambda capture site */
    const char *lname;
    Node *alias;     /* instanceof binding: the scrutinee expression */
    int is_temp;
    int elems;       /* jown: a candidate String[] / List<String> whose elements are all fresh Strings it alone owns */
    int *elem_of;    /* jown: a for-each variable over such a container: the container's `elems` flag */
    int param_ok;    /* jown: a parameter the method only reads, so a caller keeps ownership of what it passes */
    int param_elems_ok; /* jown: ...and it never takes an element out of it, so the caller's `elems` status survives the call */
    int rl_list;     /* jown/range: a List / ArrayList local whose size is tracked (see rg_prescan) */
    int drop;        /* jown proved this local is the sole owner of a fresh String/array: free it at scope end */
};

typedef enum { F_TRY, F_METHOD, F_LAMBDA, F_INIT } FrameKind;

typedef struct ExFrame {
    FrameKind kind;
    JList types;  /* caught types (F_TRY) or declared throws (F_METHOD/F_LAMBDA) */
    JList thrown; /* checked exceptions that reached this F_TRY frame */
    struct ExFrame *up;
} ExFrame;

typedef struct Ctx {
    JClass *cls;
    JMethod *method;
    int is_static;
    int in_ctor;
    JType *ret;          /* expected return type; NULL = infer (lambda) */
    JList *ret_types;    /* collects return expression types while inferring */
    int ret_seen_void;
    JList locals;        /* JLocal* stack */
    int lambda_base;     /* locals below this index belong to an enclosing function */
    ExFrame *ex;
    int loop_depth;
    int switch_depth;
    int lambda_depth;
    JList captures;      /* (JLocal*) captured by lambdas, validated at method end */
} Ctx;

/* check_expr.c */
JType *check_expr(Ctx *cx, Node *n, JType *expected);
JType *check_expr_ex(Ctx *cx, Node *n, JType *expected, int allow_type); /* allow_type: class/package names are legal results */
JType *check_cond(Ctx *cx, Node *n);
int check_assignable(Ctx *cx, Node *expr, JType *from, JType *to, const char *what);
void check_coerce(Ctx *cx, Node *expr, JType *to, const char *what); /* check_expr + assign check */
JLocal *jt_declare_local(Ctx *cx, Node *at, const char *name, JType *t, int flags, int is_param);
JLocal *lookup_local(Ctx *cx, const char *name);
void report_throw(Ctx *cx, Node *at, JType *exc);
int accessible(Ctx *cx, JClass *owner, int flags);
int const_int_value(Node *e, long *out);
JType *narrowed(JLocal *l);
JType *member_type(JType *recv, JClass *owner, JType *t);
JType *check_lambda(Ctx *cx, Node *lam, JType *target, JType **result);
int is_poly_arg(Node *n);
JType *check_switch(Ctx *cx, Node *n, JType *expected);
void check_ctor_call(Ctx *cx, Node *n);
void narrow_apply(Ctx *cx, Node *cond, int when_true, JList *undo);
void narrow_undo(JList *undo);
JType *check_new_object(Ctx *cx, Node *n, JType *expected);
void check_args_for_ctor(Ctx *cx, Node *at, JClass *c, JType *ctype, JList *args, Node *call_site, JMethod **chosen);

/* check_stmt.c */
void check_stmt(Ctx *cx, Node *n);
void check_block_body(Ctx *cx, Node *block);
int can_complete(Node *n);
void push_scope(Ctx *cx, int *mark);
void pop_scope(Ctx *cx, int mark);

/* check_class.c */
void check_method_body(JClass *c, JMethod *m);

#endif
