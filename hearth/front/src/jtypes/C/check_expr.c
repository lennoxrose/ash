#include <stdlib.h>
#include <string.h>

#include "jtypes/H/check.h"

/* Expression checking: names, members, calls with overload resolution and generic
 * inference, operators, nullability narrowing, lambdas and method references. */

#define TINT ty_prim(TY_INT)
#define TBOOL ty_prim(TY_BOOL)
#define TVOID ty_prim(TY_VOID)

/* ---------------------------------------------------------------- helpers */

JType *narrowed(JLocal *l) { return l->nonnull ? ty_with_null(l->type, 0) : l->type; }

JType *member_type(JType *recv, JClass *owner, JType *t) {
    jt_complete(owner);
    if (!owner->tparams.count) return t;
    JType *s = recv ? ty_as_super(recv, owner) : NULL;
    if (s && s->nargs == owner->tparams.count) return ty_subst(t, &owner->tparams, s->args, s->nargs);
    /* raw type: erase the owner's type parameters to their bounds */
    JType **er = j_alloc(sizeof(JType *) * (size_t)owner->tparams.count);
    for (int i = 0; i < owner->tparams.count; i++) {
        JTVar *tv = owner->tparams.items[i];
        er[i] = tv->bound ? ty_erase_bound(tv->bound) : jt_object_type();
    }
    return ty_subst(t, &owner->tparams, er, owner->tparams.count);
}

static JClass *outermost(JClass *c) {
    while (c->outer) c = c->outer;
    return c;
}

int accessible(Ctx *cx, JClass *owner, int flags) {
    if (flags & M_PUBLIC) return 1;
    if (flags & M_PRIVATE) return outermost(owner) == outermost(cx->cls);
    if (strcmp(owner->pkg, cx->cls->pkg) == 0) return 1;
    if (flags & M_PROTECTED) {
        for (JClass *c = cx->cls; c; c = c->outer) {
            if (ty_is_subclass(c, owner)) return 1;
        }
    }
    return 0;
}

static JType *recv_class(JType *t) {
    if (t->kind == TY_TVAR) return ty_erase_bound(t);
    return t;
}

static int is_stmt_expr(Node *n) {
    return n->kind == N_ASSIGN || n->kind == N_CALL || n->kind == N_NEW || n->kind == N_POSTFIX ||
           (n->kind == N_UNARY && (!strcmp(n->name, "++") || !strcmp(n->name, "--")));
}
static int is_diamond_new(Node *n) { return n->kind == N_NEW && (n->a->flags & 0x10000); }
/* arguments whose type depends on the parameter they are passed to */
/* a generic call whose type arguments could not be inferred from its own arguments (flag set by select_method) */
static int is_open_generic_call(Node *n) { return n->kind == N_CALL && (n->flags & 0x100); }
int is_poly_arg(Node *n) { return n->kind == N_LAMBDA || n->kind == N_METHODREF || is_diamond_new(n) || is_open_generic_call(n); }

static const char *what_kind(JType *t) { return t->nullable ? "nullable value" : "value"; }

/* ---------------------------------------------------------------- locals and scopes */

void push_scope(Ctx *cx, int *mark) { *mark = cx->locals.count; }
void pop_scope(Ctx *cx, int mark) { cx->locals.count = mark; }

JLocal *lookup_local(Ctx *cx, const char *name) {
    for (int i = cx->locals.count - 1; i >= 0; i--) {
        JLocal *l = cx->locals.items[i];
        if (strcmp(l->name, name) == 0) return l;
    }
    return NULL;
}

JLocal *declare_local(Ctx *cx, Node *at, const char *name, JType *t, int flags, int is_param) {
    for (int i = cx->locals.count - 1; i >= 0; i--) {
        JLocal *o = cx->locals.items[i];
        if (strcmp(o->name, name) == 0) {
            if (o->is_temp) {
                cx->locals.items[i] = cx->locals.items[cx->locals.count - 1];
                cx->locals.count--;
            } else {
                cerr(at, "variable '%s' is already defined in this scope", name);
            }
            break;
        }
    }
    JLocal *l = j_alloc(sizeof(JLocal));
    l->name = name;
    l->type = t;
    l->flags = flags;
    l->is_param = is_param;
    l->index = cx->locals.count;
    l->decl = at;
    l->lname = name;
    j_list_push(&cx->locals, l);
    return l;
}

/* ---------------------------------------------------------------- exceptions */

void report_throw(Ctx *cx, Node *at, JType *exc) {
    if (exc->kind != TY_CLASS || !ty_is_checked_exception(exc)) return;
    for (ExFrame *f = cx->ex; f; f = f->up) {
        if (f->kind == F_TRY) {
            j_list_push(&f->thrown, exc);
            for (int i = 0; i < f->types.count; i++) {
                JType *c = f->types.items[i];
                if (c->kind == TY_CLASS && ty_is_subclass(exc->cls, c->cls)) return;
            }
        } else {
            for (int i = 0; i < f->types.count; i++) {
                JType *c = f->types.items[i];
                if (c->kind == TY_CLASS && ty_is_subclass(exc->cls, c->cls)) return;
            }
            break;
        }
    }
    cerr(at, "unreported exception %s; it must be caught or declared with 'throws'", ty_str(exc));
}

/* ---------------------------------------------------------------- nullability narrowing */

typedef struct { JLocal *l; int old; } NarrowUndo;

static JLocal *local_of(Node *e) {
    return (e->kind == N_NAME && e->rk == R_LOCAL) ? e->sym : NULL;
}

static void set_nonnull(JLocal *l, JList *undo) {
    if (!l->type->nullable || l->nonnull) return;
    if (undo) {
        NarrowUndo *u = j_alloc(sizeof(NarrowUndo));
        u->l = l;
        u->old = l->nonnull;
        j_list_push(undo, u);
    }
    l->nonnull = 1;
}

void narrow_apply(Ctx *cx, Node *c, int when_true, JList *undo) {
    (void)cx;
    if (!c) return;
    if (c->kind == N_UNARY && !strcmp(c->name, "!")) {
        narrow_apply(cx, c->a, !when_true, undo);
    } else if (c->kind == N_BINARY) {
        if (!strcmp(c->name, "&&") && when_true) {
            narrow_apply(cx, c->a, 1, undo);
            narrow_apply(cx, c->b, 1, undo);
        } else if (!strcmp(c->name, "||") && !when_true) {
            narrow_apply(cx, c->a, 0, undo);
            narrow_apply(cx, c->b, 0, undo);
        } else if (!strcmp(c->name, "!=") || !strcmp(c->name, "==")) {
            int is_ne = c->name[0] == '!';
            Node *v = NULL;
            if (c->b->kind == N_LIT_NULL) v = c->a;
            else if (c->a->kind == N_LIT_NULL) v = c->b;
            JLocal *l = v ? local_of(v) : NULL;
            if (l && (when_true == is_ne)) set_nonnull(l, undo);
        }
    } else if (c->kind == N_INSTANCEOF && when_true) {
        JLocal *l = local_of(c->a);
        if (l) set_nonnull(l, undo);
    }
}

void narrow_undo(JList *undo) {
    for (int i = undo->count - 1; i >= 0; i--) {
        NarrowUndo *u = undo->items[i];
        u->l->nonnull = u->old;
    }
    undo->count = 0;
}

/* ---------------------------------------------------------------- assignment compatibility */

int const_int_value(Node *e, long *out) {
    if (e->kind == N_LIT_INT) { *out = (long)strtoull(e->name, NULL, 0); return 1; }
    if (e->kind == N_LIT_CHAR) { *out = atol(e->name); return 1; }
    if (e->kind == N_UNARY && !strcmp(e->name, "-")) {
        long v;
        if (const_int_value(e->a, &v)) { *out = -v; return 1; }
    }
    return 0;
}

int check_assignable(Ctx *cx, Node *expr, JType *from, JType *to, const char *what) {
    (void)cx;
    if (from->kind == TY_ERROR || to->kind == TY_ERROR) return 1;
    if (ty_assignable(from, to, 0)) return 1;
    long v;
    if (expr && ty_is_prim(to) && to->kind != TY_BOOL && to->kind != TY_INT && to->kind != TY_LONG &&
        (from->kind == TY_INT || from->kind == TY_CHAR || from->kind == TY_SHORT || from->kind == TY_BYTE) &&
        const_int_value(expr, &v)) {
        long lo = to->kind == TY_BYTE ? -128 : to->kind == TY_SHORT ? -32768 : to->kind == TY_CHAR ? 0 : 0;
        long hi = to->kind == TY_BYTE ? 127 : to->kind == TY_SHORT ? 32767 : to->kind == TY_CHAR ? 65535 : 0;
        if (to->kind == TY_FLOAT || to->kind == TY_DOUBLE) return 1;
        if (v >= lo && v <= hi) return 1;
    }
    if (from->kind == TY_VOID) {
        cerr(expr, "%s has type void; a value is required", what);
    } else if (from->kind == TY_NULL) {
        cerr(expr, "null cannot be used for %s of non-null type %s; declare it as %s?", what, ty_str(to), ty_str(to));
    } else if (from->nullable && !to->nullable && ty_assignable(from, to, 1)) {
        cerr(expr, "possible null: %s of nullable type %s cannot be used where non-null %s is required; check for null first",
             what, ty_str(from), ty_str(to));
    } else if (ty_is_prim(from) && ty_is_prim(to) && from->kind != TY_BOOL && to->kind != TY_BOOL) {
        cerr(expr, "possible lossy conversion in %s from %s to %s; use an explicit cast", what, ty_str(from), ty_str(to));
    } else {
        cerr(expr, "incompatible types in %s: %s cannot be converted to %s", what, ty_str(from), ty_str(to));
    }
    return 0;
}

void check_coerce(Ctx *cx, Node *expr, JType *to, const char *what) {
    JType *t = check_expr(cx, expr, to);
    check_assignable(cx, expr, t, to, what);
}

JType *check_cond(Ctx *cx, Node *n) {
    JType *t = check_expr(cx, n, TBOOL);
    if (t->kind != TY_ERROR && !(ty_unbox(t)->kind == TY_BOOL && !t->nullable)) {
        cerr(n, "condition must be of type bool, found %s", ty_str(t));
    }
    return t;
}

/* ---------------------------------------------------------------- generic inference */

static int tv_index(JMethod *m, JType *t) {
    if (t->kind != TY_TVAR || t->tv->owner_m != m) return -1;
    for (int i = 0; i < m->tparams.count; i++) {
        if (m->tparams.items[i] == t->tv) return i;
    }
    return -1;
}

static int mentions_free(JType *t, JMethod *m, JType **bind) {
    switch (t->kind) {
        case TY_TVAR: {
            int i = tv_index(m, t);
            return i >= 0 && !bind[i];
        }
        case TY_ARRAY: return mentions_free(t->elem, m, bind);
        case TY_CLASS:
            for (int i = 0; i < t->nargs; i++) if (mentions_free(t->args[i], m, bind)) return 1;
            return 0;
        default: return 0;
    }
}

static void unify(JType *p, JType *a, JMethod *m, JType **bind) {
    if (!a || a->kind == TY_NULL || a->kind == TY_ERROR || a->kind == TY_VOID) return;
    int i = tv_index(m, p);
    if (i >= 0) {
        JType *b = ty_is_prim(a) ? ty_box(a) : a;
        if (!bind[i]) bind[i] = b;
        else if (!ty_assignable(b, bind[i], 1) && ty_assignable(bind[i], b, 1)) bind[i] = b;
        else if (bind[i]->nullable != b->nullable && ty_assignable(b, bind[i], 1)) bind[i] = ty_with_null(bind[i], bind[i]->nullable || b->nullable);
        return;
    }
    if (p->kind == TY_ARRAY && a->kind == TY_ARRAY) {
        unify(p->elem, a->elem, m, bind);
    } else if (p->kind == TY_CLASS && p->nargs && (a->kind == TY_CLASS || a->kind == TY_TVAR)) {
        JType *s = ty_as_super(a, p->cls);
        if (s && s->nargs == p->nargs) {
            for (int k = 0; k < p->nargs; k++) unify(p->args[k], s->args[k], m, bind);
        }
    }
}

static JType *fill_bind(JMethod *m, JType **bind, int erase_unbound, JType ***out) {
    int n = m->tparams.count;
    JType **b = j_alloc(sizeof(JType *) * (size_t)(n ? n : 1));
    for (int i = 0; i < n; i++) {
        if (bind[i]) b[i] = bind[i];
        else if (erase_unbound) {
            JTVar *tv = m->tparams.items[i];
            b[i] = tv->bound ? ty_erase_bound(tv->bound) : jt_object_type();
        } else b[i] = ty_tvar(m->tparams.items[i]);
    }
    *out = b;
    return NULL;
}

static JType *msubst(JType *t, JMethod *m, JType **bind) {
    if (!m->tparams.count) return t;
    JType **b;
    fill_bind(m, bind, 0, &b);
    return ty_subst(t, &m->tparams, b, m->tparams.count);
}

/* ---------------------------------------------------------------- overload resolution */

typedef struct {
    JMethod *m;
    JType **bind;
    int phase;
} Cand;

static JType *formal(JMethod *m, JType *recv, int i, int phase) {
    int np = m->params.count;
    JType *pt;
    if (phase == 3 && i >= np - 1) pt = ((JParam *)m->params.items[np - 1])->type->elem;
    else pt = ((JParam *)m->params.items[i])->type;
    return member_type(recv, m->cls, pt);
}

static int lambda_arity_ok(Node *a, JType *pt) {
    if (is_open_generic_call(a)) return pt->kind == TY_CLASS || pt->kind == TY_TVAR;
    if (is_diamond_new(a)) {
        if (pt->kind == TY_TVAR) return 1;
        JType *nt = jt_resolve_type(a->a, NULL, NULL, 0);
        Node *tn = a->a;
        (void)tn;
        if (nt->kind != TY_CLASS) {
            /* resolved lazily against the caller's scope during completion */
            return pt->kind == TY_CLASS;
        }
        return pt->kind == TY_CLASS && ty_is_subclass(nt->cls, pt->cls);
    }
    if (!jt_is_functional(pt)) return 0;
    if (a->kind == N_METHODREF) return 1;
    JMethod *sam = jt_find_sam(pt->cls);
    return sam && sam->params.count == a->l1.count;
}

static int applicable(JMethod *m, JType *recv, JList *args, int phase, Cand *out) {
    int n = args->count, np = m->params.count;
    if (phase == 3) {
        if (!m->varargs || n < np - 1) return 0;
    } else if (n != np) {
        return 0;
    }
    JType **bind = j_alloc(sizeof(JType *) * (size_t)(m->tparams.count ? m->tparams.count : 1));
    for (int i = 0; i < n; i++) {
        Node *a = args->items[i];
        JType *pt = formal(m, recv, i, phase);
        if (is_poly_arg(a)) {
            if (!lambda_arity_ok(a, msubst(pt, m, bind))) return 0;
            continue;
        }
        unify(pt, a->type, m, bind);
    }
    for (int i = 0; i < n; i++) {
        Node *a = args->items[i];
        if (is_poly_arg(a)) continue;
        JType *pt = msubst(formal(m, recv, i, phase), m, bind);
        if (mentions_free(pt, m, bind)) continue;
        JType *at = a->type;
        if (at->kind == TY_ERROR) continue;
        if (phase == 1 && ty_is_prim(at) != ty_is_prim(pt) && at->kind != TY_NULL) return 0;
        long v;
        if (ty_assignable(at, pt, 0)) continue;
        (void)v;
        return 0;
    }
    for (int i = 0; i < m->tparams.count; i++) {
        JTVar *tv = m->tparams.items[i];
        if (tv->bound && bind[i]) {
            JType *bd = msubst(tv->bound, m, bind);
            if (!ty_assignable(bind[i], bd, 1)) return 0;
        }
    }
    out->m = m;
    out->bind = bind;
    out->phase = phase;
    return 1;
}

static int more_specific(JMethod *a, JMethod *b) {
    int n = a->params.count < b->params.count ? a->params.count : b->params.count;
    for (int i = 0; i < n; i++) {
        JType *x = ty_erase_bound(((JParam *)a->params.items[i])->type);
        JType *y = ty_erase_bound(((JParam *)b->params.items[i])->type);
        if (!ty_assignable(x, ty_with_null(y, 1), 1)) return 0;
    }
    return 1;
}

static const char *sig_str(JMethod *m) {
    JBuf b = {0};
    j_buf_add(&b, m->is_ctor ? m->cls->name : m->name);
    j_buf_addc(&b, '(');
    for (int i = 0; i < m->params.count; i++) {
        if (i) j_buf_add(&b, ", ");
        j_buf_add(&b, ty_str(((JParam *)m->params.items[i])->type));
    }
    j_buf_addc(&b, ')');
    return b.data;
}

static const char *args_str(JList *args) {
    JBuf b = {0};
    j_buf_addc(&b, '(');
    for (int i = 0; i < args->count; i++) {
        Node *a = args->items[i];
        if (i) j_buf_add(&b, ", ");
        j_buf_add(&b, is_poly_arg(a) ? (a->kind == N_LAMBDA ? "lambda" : a->kind == N_METHODREF ? "method reference" : a->kind == N_CALL ? "generic call" : "new <>") : ty_str(a->type));
    }
    j_buf_addc(&b, ')');
    return b.data;
}

/* Picks the best method for already-typed args; complete lambdas, infer type arguments and
 * return the instantiated result type through *ret. */
static JMethod *select_method(Ctx *cx, Node *at, JList *cands, JList *args, JType *recv, JType *expected,
                              const char *what, JType **ret, JType ***bind_out) {
    Cand found[64];
    int nf = 0;
    for (int phase = 1; phase <= 3 && nf == 0; phase++) {
        for (int i = 0; i < cands->count && nf < 64; i++) {
            Cand c;
            if (applicable(cands->items[i], recv, args, phase, &c)) found[nf++] = c;
        }
    }
    if (nf == 0) {
        for (int i = 0; i < args->count; i++) {
            Node *a = args->items[i];
            if (!is_poly_arg(a) && a->type->kind == TY_ERROR) { *ret = ty_error(); return NULL; }
        }
        JMethod *only = NULL;
        for (int i = 0; i < cands->count; i++) {
            JMethod *m = cands->items[i];
            if (m->params.count == args->count) {
                if (only) { only = NULL; break; }
                only = m;
            }
        }
        int reported = 0;
        for (int i = 0; only && i < args->count && !reported; i++) {
            Node *a = args->items[i];
            if (is_poly_arg(a)) continue;
            JType *pt = formal(only, recv, i, 2);
            if (a->type->nullable && ty_assignable(a->type, pt, 1) && !ty_assignable(a->type, pt, 0)) {
                cerr(a, "possible null: argument %d of nullable type %s cannot be passed to non-null parameter '%s' of type %s; check for null first",
                     i + 1, ty_str(a->type), ((JParam *)only->params.items[i])->name, ty_str(pt));
                reported = 1;
            }
        }
        if (reported) {
            /* already explained */
        } else if (cands->count == 1) {
            JMethod *m = cands->items[0];
            cerr(at, "%s %s cannot be applied to %s", what, sig_str(m), args_str(args));
        } else {
            JBuf b = {0};
            for (int i = 0; i < cands->count; i++) {
                j_buf_add(&b, "\n    candidate: ");
                j_buf_add(&b, sig_str(cands->items[i]));
            }
            cerr(at, "no %s matches %s%s", what, args_str(args), b.data ? b.data : "");
        }
        *ret = ty_error();
        return NULL;
    }
    int best = 0;
    if (nf > 1) {
        int ok = 0;
        for (int i = 0; i < nf; i++) {
            int all = 1;
            for (int j = 0; j < nf; j++) {
                if (i != j && !more_specific(found[i].m, found[j].m)) all = 0;
            }
            if (all) { best = i; ok = 1; break; }
        }
        for (int i = 0; i < args->count && !ok; i++) {
            Node *a = args->items[i];
            if (!is_poly_arg(a) && a->type->kind == TY_ERROR) ok = 1; /* an earlier error already explains it */
        }
        if (!ok) {
            cerr(at, "ambiguous %s call: both %s and %s match %s", what, sig_str(found[0].m), sig_str(found[1].m), args_str(args));
            *ret = ty_error();
            return NULL;
        }
    }
    JMethod *m = found[best].m;
    JType **bind = found[best].bind;
    int phase = found[best].phase;
    /* return-type driven inference (e.g. List<String> x = identity(...)) */
    JType *rt = member_type(recv, m->cls, m->ret);
    if (!expected && m->tparams.count && at->kind == N_CALL && mentions_free(rt, m, bind)) at->flags |= 0x100;
    if (expected && m->tparams.count && mentions_free(rt, m, bind)) unify(rt, expected, m, bind);
    /* complete lambdas and method references now that argument-derived bindings exist */
    for (int i = 0; i < args->count; i++) {
        Node *a = args->items[i];
        if (!is_poly_arg(a)) continue;
        JType *pt = msubst(formal(m, recv, i, phase), m, bind);
        if (is_diamond_new(a) || is_open_generic_call(a)) {
            JType *nt = check_expr(cx, a, pt->kind == TY_TVAR ? NULL : pt);
            unify(formal(m, recv, i, phase), nt, m, bind);
            continue;
        }
        JType *res = NULL;
        JType *tt = check_lambda(cx, a, pt, &res);
        if (res && tt->kind == TY_CLASS) {
            JMethod *sam = jt_find_sam(tt->cls);
            if (sam) unify(member_type(pt, sam->cls, sam->ret), res, m, bind);
        }
    }
    /* re-instantiate lambda node types with the final bindings */
    for (int i = 0; i < args->count; i++) {
        Node *a = args->items[i];
        if (is_poly_arg(a) && !is_diamond_new(a) && !is_open_generic_call(a)) a->type = msubst(formal(m, recv, i, phase), m, bind);
    }
    JType **fb;
    fill_bind(m, bind, 1, &fb);
    *ret = m->tparams.count ? ty_subst(rt, &m->tparams, fb, m->tparams.count) : rt;
    if (bind_out) *bind_out = fb;
    for (int i = 0; i < m->throws.count; i++) {
        JType *tt = member_type(recv, m->cls, m->throws.items[i]);
        if (m->tparams.count) tt = ty_subst(tt, &m->tparams, fb, m->tparams.count);
        report_throw(cx, at, tt);
    }
    if (m->flags & M_PRIVATE ? 0 : 0) {}
    if (!accessible(cx, m->cls, m->flags)) {
        cerr(at, "%s '%s' has %s access in %s", what, m->is_ctor ? m->cls->name : m->name,
             (m->flags & M_PRIVATE) ? "private" : (m->flags & M_PROTECTED) ? "protected" : "package-private", m->cls->name);
    }
    return m;
}

static void check_args(Ctx *cx, JList *args) {
    for (int i = 0; i < args->count; i++) {
        Node *a = args->items[i];
        if (is_poly_arg(a)) continue;
        check_expr(cx, a, NULL);
        if (a->type->kind == TY_VOID) cerr(a, "a void expression cannot be used as an argument");
    }
}

void check_args_for_ctor(Ctx *cx, Node *at, JClass *c, JType *ctype, JList *args, Node *call_site, JMethod **chosen) {
    (void)call_site;
    jt_complete(c);
    check_args(cx, args);
    JType *ret;
    JMethod *m = select_method(cx, at, &c->ctors, args, ctype, NULL, "constructor", &ret, NULL);
    if (chosen) *chosen = m;
}

/* ---------------------------------------------------------------- lambdas and method references */

static JType *box_result(JType *t) { return ty_is_prim(t) ? ty_box(t) : t; }

JType *check_lambda(Ctx *cx, Node *lam, JType *target, JType **result) {
    if (result) *result = NULL;
    if (!target || !jt_is_functional(target)) {
        if (target && target->kind != TY_ERROR) cerr(lam, "%s is not a functional interface; a lambda needs one as its target type", ty_str(target));
        else if (!target) cerr(lam, "cannot infer the type of this lambda; it needs a functional interface target type");
        lam->type = ty_error();
        return lam->type;
    }
    JMethod *sam = jt_find_sam(target->cls);
    lam->sym = sam;
    lam->type = target;
    if (lam->kind == N_METHODREF) {
        /* Type::method, expr::method, Type::new */
        Node *tn = lam->a;
        JType *recv = check_expr_ex(cx, tn, NULL, 1);
        int is_type = tn->rk == R_CLASS;
        int np = sam->params.count;
        JType **pts = j_alloc(sizeof(JType *) * (size_t)(np ? np : 1));
        for (int i = 0; i < np; i++) pts[i] = member_type(target, sam->cls, ((JParam *)sam->params.items[i])->type);
        JMethod *found = NULL;
        JType *rt = NULL;
        if (!strcmp(lam->name, "new")) {
            if (!is_type) { cerr(lam, "'::new' needs a class"); return ty_error(); }
            JClass *c = tn->sym;
            jt_complete(c);
            for (int i = 0; i < c->ctors.count && !found; i++) {
                JMethod *k = c->ctors.items[i];
                if (k->params.count != np) continue;
                int ok = 1;
                for (int j = 0; j < np; j++) {
                    if (!ty_assignable(pts[j], ((JParam *)k->params.items[j])->type, 1) && !(pts[j]->kind == TY_TVAR)) ok = 0;
                }
                if (ok) found = k;
            }
            rt = recv;
        } else {
            JClass *c = NULL;
            JType *rcv = recv;
            if (is_type) c = tn->sym;
            else if (recv->kind == TY_CLASS || recv->kind == TY_TVAR) { rcv = recv_class(recv); c = rcv->cls; }
            if (!c) { cerr(lam, "cannot take a method reference on type %s", ty_str(recv)); return ty_error(); }
            JList all = {0};
            jt_collect_methods(c, lam->name, &all);
            for (int i = 0; i < all.count && !found; i++) {
                JMethod *k = all.items[i];
                int stat = (k->flags & M_STATIC) != 0;
                int want = (is_type && !stat) ? np - 1 : np;
                if (k->params.count != want) continue;
                int off = (is_type && !stat) ? 1 : 0;
                int ok = 1;
                for (int j = 0; j < want; j++) {
                    JType *pt = member_type(rcv, k->cls, ((JParam *)k->params.items[j])->type);
                    JType *at2 = pts[j + off];
                    if (at2->kind == TY_TVAR || pt->kind == TY_TVAR) continue;
                    if (!ty_assignable(at2, ty_with_null(pt, 1), 1)) ok = 0;
                }
                if (ok) {
                    found = k;
                    rt = member_type(rcv, k->cls, k->ret);
                    if (k->tparams.count) {
                        JType **eb;
                        JType **empty = j_alloc(sizeof(JType *) * (size_t)k->tparams.count);
                        fill_bind(k, empty, 1, &eb);
                        rt = ty_subst(rt, &k->tparams, eb, k->tparams.count);
                    }
                    for (int j = 0; j < k->throws.count; j++) report_throw(cx, lam, k->throws.items[j]);
                }
            }
        }
        if (!found) {
            cerr(lam, "no method '%s' of %s matches the signature of %s", lam->name, ty_str(recv), ty_str(target));
            lam->type = ty_error();
            return lam->type;
        }
        lam->sym2 = found;
        if (result && rt && rt->kind != TY_VOID) *result = box_result(rt);
        return target;
    }
    /* lambda expression */
    int np = sam->params.count;
    if (lam->l1.count != np) {
        cerr(lam, "lambda has %d parameter(s) but %s expects %d", lam->l1.count, ty_str(target), np);
        lam->type = ty_error();
        return lam->type;
    }
    JType *sam_ret = member_type(target, sam->cls, sam->ret);
    int mark;
    push_scope(cx, &mark);
    int saved_base = cx->lambda_base;
    int saved_loop = cx->loop_depth, saved_switch = cx->switch_depth;
    JType *saved_ret = cx->ret;
    JList *saved_rt = cx->ret_types;
    int saved_rv = cx->ret_seen_void;
    ExFrame *saved_ex = cx->ex;
    cx->lambda_base = cx->locals.count;
    cx->loop_depth = cx->switch_depth = 0;
    cx->lambda_depth++;
    ExFrame fr = {0};
    fr.kind = F_LAMBDA;
    fr.up = NULL; /* a lambda body is its own checked-exception boundary */
    for (int i = 0; i < sam->throws.count; i++) j_list_push(&fr.types, member_type(target, sam->cls, sam->throws.items[i]));
    cx->ex = &fr;
    for (int i = 0; i < np; i++) {
        Node *p = lam->l1.items[i];
        JType *pt = member_type(target, sam->cls, ((JParam *)sam->params.items[i])->type);
        if (p->a) {
            JType *dt = jt_resolve_type(p->a, cx->cls, cx->method, 1);
            if (!ty_assignable(pt, dt, 1) && pt->kind != TY_TVAR) cerr(p, "lambda parameter '%s' has type %s but %s expects %s", p->name, ty_str(dt), ty_str(target), ty_str(pt));
            pt = dt;
        } else if (pt->kind == TY_TVAR && pt->tv->owner_m) {
            pt = ty_erase_bound(pt); /* uninferred method type variable */
        }
        JLocal *l = declare_local(cx, p, p->name, pt, p->flags, 1);
        l->has_init = 1;
    }
    JList rts = {0};
    JType *res = NULL;
    if (lam->b->kind == N_BLOCK) {
        int infer = sam_ret->kind == TY_TVAR || (sam_ret->kind == TY_CLASS && sam_ret->nargs && sam_ret->args[0]->kind == TY_TVAR && 0);
        for (int i = 0, any = 0; i < 1 && !any; i++) any = 1;
        cx->ret = (sam_ret->kind == TY_VOID || infer == 0) ? sam_ret : NULL;
        if (sam_ret->kind == TY_TVAR && sam_ret->tv->owner_m) cx->ret = NULL;
        cx->ret_types = &rts;
        cx->ret_seen_void = 0;
        check_block_body(cx, lam->b);
        if (cx->ret && cx->ret->kind != TY_VOID && can_complete(lam->b)) cerr(lam, "lambda body is missing a return statement");
        if (rts.count) {
            res = rts.items[0];
            for (int i = 1; i < rts.count; i++) {
                JType *o = rts.items[i];
                if (ty_assignable(res, o, 1)) res = o;
            }
        }
    } else {
        JType *want = (sam_ret->kind == TY_VOID || (sam_ret->kind == TY_TVAR && sam_ret->tv->owner_m)) ? NULL : sam_ret;
        JType *bt = check_expr(cx, lam->b, want);
        if (sam_ret->kind == TY_VOID) {
            if (!is_stmt_expr(lam->b) && bt->kind != TY_ERROR) cerr(lam->b, "lambda body is not a statement; %s expects no return value", ty_str(target));
        } else if (want) {
            check_assignable(cx, lam->b, bt, want, "lambda result");
        }
        if (bt->kind != TY_VOID) res = bt;
    }
    cx->ex = saved_ex;
    cx->lambda_depth--;
    cx->loop_depth = saved_loop;
    cx->switch_depth = saved_switch;
    cx->lambda_base = saved_base;
    cx->ret = saved_ret;
    cx->ret_types = saved_rt;
    cx->ret_seen_void = saved_rv;
    pop_scope(cx, mark);
    if (result && res && res->kind != TY_NULL) *result = box_result(res);
    return target;
}

/* ---------------------------------------------------------------- names and members */

static JType *class_ref(Node *n, JClass *c) {
    n->rk = R_CLASS;
    n->sym = c;
    jt_complete(c);
    return ty_class(c, NULL, 0);
}

static JType *resolve_simple_type_name(Ctx *cx, Node *n) {
    Node *tn = node_new(N_TYPE, n->path, n->line, n->col);
    tn->name = n->name;
    JType *t = jt_resolve_type(tn, cx->cls, cx->method, 0);
    if (t->kind == TY_CLASS) return class_ref(n, t->cls);
    return NULL;
}

static JType *field_value_type(JType *recv, JField *f) {
    return member_type(recv, f->cls, f->type);
}

static void deref_check(Node *at, Node *target, JType *t) {
    if (t->nullable) {
        const char *nm = target->kind == N_NAME ? target->name : NULL;
        if (nm) cerr(at, "possible null dereference: '%s' has nullable type %s; check it for null first", nm, ty_str(t));
        else cerr(at, "possible null dereference: the %s of type %s may be null; check it for null first", what_kind(t), ty_str(t));
    }
}

static JType *check_name(Ctx *cx, Node *n, int allow_type) {
    JLocal *l = lookup_local(cx, n->name);
    if (l) {
        n->rk = R_LOCAL;
        n->sym = l;
        if (l->index < cx->lambda_base) {
            if (!l->captured_at) l->captured_at = n;
            j_list_push(&cx->captures, l);
        }
        return narrowed(l);
    }
    for (JClass *c = cx->cls; c; c = c->outer) {
        JField *f = jt_find_field(c, n->name);
        if (!f) continue;
        if (f->flags & M_STATIC) {
            n->rk = f->is_enum_const ? R_ENUM_CONST : R_STATIC_FIELD;
            n->sym = f;
            if (!accessible(cx, f->cls, f->flags)) cerr(n, "field '%s' has private access in %s", f->name, f->cls->name);
            return f->type;
        }
        if (cx->is_static || c != cx->cls) {
            cerr(n, "non-static field '%s' cannot be referenced from a static context", n->name);
            return ty_error();
        }
        n->rk = R_FIELD;
        n->sym = f;
        if (!accessible(cx, f->cls, f->flags)) cerr(n, "field '%s' has private access in %s", f->name, f->cls->name);
        return field_value_type(cx->cls->self_type, f);
    }
    JType *ct = resolve_simple_type_name(cx, n);
    if (ct) {
        if (!allow_type) cerr(n, "'%s' is a type, not a value", n->name);
        return ct;
    }
    if (allow_type && n->name[0] >= 'a' && n->name[0] <= 'z') {
        n->rk = R_PACKAGE;
        n->sym2 = (void *)n->name;
        return ty_error();
    }
    cerr(n, "cannot find symbol '%s'", n->name);
    return ty_error();
}

static JType *check_fieldacc(Ctx *cx, Node *n, int allow_type) {
    Node *tg = n->a;
    JType *tt = check_expr_ex(cx, tg, NULL, 1);
    if (tt->kind == TY_ERROR && tg->rk != R_PACKAGE) return ty_error();
    if (tg->rk == R_PACKAGE) {
        const char *fq = j_fmt("%s.%s", (const char *)tg->sym2, n->name);
        JClass *c = jt_find_class(fq);
        if (c) return class_ref(n, c);
        n->rk = R_PACKAGE;
        n->sym2 = (void *)fq;
        if (!allow_type) cerr(n, "cannot find symbol '%s'", fq);
        return ty_error();
    }
    if (tg->rk == R_CLASS) {
        JClass *c = tg->sym;
        JField *f = jt_find_field(c, n->name);
        if (f) {
            if (!(f->flags & M_STATIC)) {
                cerr(n, "non-static field '%s' cannot be referenced from a static context", n->name);
                return ty_error();
            }
            if (!accessible(cx, f->cls, f->flags)) cerr(n, "field '%s' has private access in %s", f->name, f->cls->name);
            n->rk = f->is_enum_const ? R_ENUM_CONST : R_STATIC_FIELD;
            n->sym = f;
            return f->type;
        }
        JClass *nc = jt_find_class(j_fmt("%s.%s", c->qname, n->name));
        if (nc) {
            if (!allow_type) cerr(n, "'%s' is a type, not a value", n->name);
            return class_ref(n, nc);
        }
        cerr(n, "cannot find symbol '%s' in %s", n->name, c->name);
        return ty_error();
    }
    if (tt->kind == TY_ARRAY && !strcmp(n->name, "length")) {
        deref_check(n, tg, tt);
        n->rk = R_ARRAYLEN;
        return TINT;
    }
    deref_check(n, tg, tt);
    JType *rc = recv_class(tt);
    if (rc->kind != TY_CLASS) {
        cerr(n, "type %s has no field '%s'", ty_str(tt), n->name);
        return ty_error();
    }
    JField *f = jt_find_field(rc->cls, n->name);
    if (!f) {
        cerr(n, "cannot find field '%s' in %s", n->name, ty_str(tt));
        return ty_error();
    }
    if (!accessible(cx, f->cls, f->flags)) cerr(n, "field '%s' has private access in %s", f->name, f->cls->name);
    n->rk = (f->flags & M_STATIC) ? R_STATIC_FIELD : (tg->kind == N_THIS ? R_THIS_FIELD : R_FIELD);
    n->sym = f;
    return field_value_type(rc, f);
}

/* ---------------------------------------------------------------- calls */

static JType *check_call(Ctx *cx, Node *n, JType *expected) {
    check_args(cx, &n->l1);
    JList cands = {0};
    JType *recv = NULL;
    int static_only = 0;
    int unqualified = 0;
    if (!n->a) {
        unqualified = 1;
        for (JClass *c = cx->cls; c; c = c->outer) {
            jt_collect_methods(c, n->name, &cands);
            if (cands.count) { recv = c->self_type; static_only = c != cx->cls; break; }
        }
        if (!cands.count) {
            cerr(n, "cannot find method '%s'", n->name);
            return ty_error();
        }
    } else if (n->flags == 1) {
        if (!cx->cls->super) { cerr(n, "no superclass"); return ty_error(); }
        recv = cx->cls->super;
        jt_collect_methods(recv->cls, n->name, &cands);
        if (cx->is_static) cerr(n, "'super' cannot be used in a static context");
    } else {
        JType *tt = check_expr_ex(cx, n->a, NULL, 1);
        if (tt->kind == TY_ERROR && n->a->rk != R_CLASS) return ty_error();
        if (n->a->rk == R_PACKAGE) { cerr(n->a, "cannot find symbol '%s'", (const char *)n->a->sym2); return ty_error(); }
        if (n->a->rk == R_CLASS) {
            static_only = 1;
            recv = tt;
            jt_collect_methods(((JClass *)n->a->sym), n->name, &cands);
        } else {
            deref_check(n, n->a, tt);
            recv = recv_class(tt);
            if (recv->kind == TY_ARRAY) recv = jt_object_type();
            if (recv->kind != TY_CLASS) {
                cerr(n, "cannot call method '%s' on type %s", n->name, ty_str(tt));
                return ty_error();
            }
            jt_collect_methods(recv->cls, n->name, &cands);
        }
        if (!cands.count) {
            cerr(n, "cannot find method '%s' in %s", n->name, ty_str(tt));
            return ty_error();
        }
    }
    JType *ret;
    JType **bind = NULL;
    JMethod *m = select_method(cx, n, &cands, &n->l1, recv, expected, "method", &ret, &bind);
    if (!m) return ty_error();
    n->sym = m;
    if (m->flags & M_STATIC) {
        n->rk = R_STATIC_METHOD;
    } else {
        if (static_only || (unqualified && cx->is_static)) {
            cerr(n, "non-static method '%s' cannot be referenced from a static context", m->name);
            return ty_error();
        }
        if ((m->flags & M_ABSTRACT) && n->flags == 1) {
            cerr(n, "abstract method '%s' cannot be called through 'super'", m->name);
            return ty_error();
        }
        n->rk = n->flags == 1 ? R_SUPER_METHOD : R_METHOD;
    }
    return ret;
}

void check_ctor_call(Ctx *cx, Node *n) {
    int is_this = !strcmp(n->name, "this");
    JClass *target = is_this ? cx->cls : (cx->cls->super ? cx->cls->super->cls : NULL);
    if (!cx->in_ctor) { cerr(n, "'%s(...)' is only allowed as the first statement of a constructor", n->name); return; }
    if (!target) return;
    JType *ctype = is_this ? cx->cls->self_type : cx->cls->super;
    JMethod *m = NULL;
    check_args_for_ctor(cx, n, target, ctype, &n->l1, n, &m);
    n->sym = m;
    n->rk = R_CTOR;
}

JType *check_new_object(Ctx *cx, Node *n, JType *expected) {
    Node *tn = n->a;
    JType *t = jt_resolve_type(tn, cx->cls, cx->method, 1);
    if (t->kind != TY_CLASS) {
        if (t->kind != TY_ERROR) cerr(n, "cannot instantiate type %s", ty_str(t));
        check_args(cx, &n->l1);
        return ty_error();
    }
    JClass *c = t->cls;
    jt_complete(c);
    if (c->kind == N_INTERFACE || (c->flags & M_ABSTRACT)) cerr(n, "%s is abstract; it cannot be instantiated", c->name);
    else if (c->kind == N_ENUM) cerr(n, "enum %s cannot be instantiated with 'new'", c->name);
    if (tn->flags & 0x10000) { /* diamond: infer arguments from the target type */
        int nt = c->tparams.count;
        JType **targs = j_alloc(sizeof(JType *) * (size_t)(nt ? nt : 1));
        int got = 0;
        if (expected && expected->kind == TY_CLASS && expected->nargs) {
            JType *self = c->self_type;
            JType *as = ty_as_super(self, expected->cls);
            if (as && as->nargs == expected->nargs) {
                for (int i = 0; i < nt; i++) targs[i] = NULL;
                for (int k = 0; k < as->nargs; k++) {
                    if (as->args[k]->kind == TY_TVAR) {
                        for (int i = 0; i < nt; i++) {
                            if (c->tparams.items[i] == as->args[k]->tv) targs[i] = expected->args[k];
                        }
                    }
                }
                got = 1;
            }
        }
        for (int i = 0; i < nt; i++) {
            if (!got || !targs[i]) {
                JTVar *tv = c->tparams.items[i];
                targs[i] = tv->bound ? ty_erase_bound(tv->bound) : jt_object_type();
            }
        }
        t = ty_class(c, targs, nt);
    } else if (c->tparams.count && !t->nargs) {
        cerr(n, "generic class %s needs type arguments; write %s<...> or use the diamond '<>'", c->name, c->name);
    }
    JMethod *ctor = NULL;
    check_args_for_ctor(cx, n, c, t, &n->l1, n, &ctor);
    n->sym = ctor;
    n->rk = R_CTOR;
    return ty_with_null(t, 0);
}

/* ---------------------------------------------------------------- operators */

static int is_bool(JType *t) { return ty_unbox(t)->kind == TY_BOOL && !t->nullable; }

static void bad_operands(Node *n, JType *a, JType *b) {
    cerr(n, "operator '%s' cannot be applied to %s and %s", n->name, ty_str(a), ty_str(b));
}

static void non_null_operand(Node *n, Node *e, JType *t) {
    if (t->nullable && ty_is_prim(ty_unbox(t))) cerr(e, "possible null: nullable %s cannot be used with operator '%s'; check for null first", ty_str(t), n->name);
}

static JType *check_binary(Ctx *cx, Node *n) {
    const char *op = n->name;
    if (!strcmp(op, "&&") || !strcmp(op, "||")) {
        JType *a = check_expr(cx, n->a, TBOOL);
        JList undo = {0};
        narrow_apply(cx, n->a, op[0] == '&', &undo);
        JType *b = check_expr(cx, n->b, TBOOL);
        narrow_undo(&undo);
        if (a->kind == TY_ERROR || b->kind == TY_ERROR) return TBOOL;
        if (!is_bool(a) || !is_bool(b)) bad_operands(n, a, b);
        return TBOOL;
    }
    JType *a = check_expr(cx, n->a, NULL);
    JType *b = check_expr(cx, n->b, NULL);
    if (a->kind == TY_ERROR || b->kind == TY_ERROR) {
        if (!strcmp(op, "+") && (ty_is_string(a) || ty_is_string(b))) return jt_string_type();
        return (op[0] == '<' || op[0] == '>' || !strcmp(op, "==") || !strcmp(op, "!=")) && strcmp(op, "<<") && strcmp(op, ">>") && strcmp(op, ">>>") ? TBOOL : ty_error();
    }
    if (a->kind == TY_VOID || b->kind == TY_VOID) {
        cerr(n, "operator '%s' cannot be applied to a void expression", op);
        return ty_error();
    }
    if (!strcmp(op, "+") && (ty_is_string(a) || ty_is_string(b))) return jt_string_type();
    if (!strcmp(op, "+") || !strcmp(op, "-") || !strcmp(op, "*") || !strcmp(op, "/") || !strcmp(op, "%")) {
        JType *r = ty_binary_numeric(a, b);
        if (!r) { bad_operands(n, a, b); return ty_error(); }
        non_null_operand(n, n->a, a);
        non_null_operand(n, n->b, b);
        return r;
    }
    if (!strcmp(op, "<<") || !strcmp(op, ">>") || !strcmp(op, ">>>")) {
        JType *ua = ty_unbox(a), *ub = ty_unbox(b);
        if (!ty_is_integral(ua) || !ty_is_integral(ub)) { bad_operands(n, a, b); return ty_error(); }
        return ua->kind == TY_LONG ? ty_prim(TY_LONG) : TINT;
    }
    if (!strcmp(op, "<") || !strcmp(op, "<=") || !strcmp(op, ">") || !strcmp(op, ">=")) {
        if (!ty_binary_numeric(a, b)) { bad_operands(n, a, b); return ty_error(); }
        non_null_operand(n, n->a, a);
        non_null_operand(n, n->b, b);
        return TBOOL;
    }
    if (!strcmp(op, "==") || !strcmp(op, "!=")) {
        if (ty_is_prim(a) || ty_is_prim(b)) {
            JType *ua = ty_unbox(a), *ub = ty_unbox(b);
            if ((ty_is_numeric(ua) && ty_is_numeric(ub)) || (ua->kind == TY_BOOL && ub->kind == TY_BOOL)) {
                non_null_operand(n, n->a, a);
                non_null_operand(n, n->b, b);
                return TBOOL;
            }
            bad_operands(n, a, b);
            return TBOOL;
        }
        if (a->kind == TY_NULL || b->kind == TY_NULL) {
            JType *o = a->kind == TY_NULL ? b : a;
            if (o->kind != TY_NULL && !o->nullable) {
                cerr(n, "comparison with null is always %s: %s is a non-null type", op[0] == '=' ? "false" : "true", ty_str(o));
            }
            return TBOOL;
        }
        if (!ty_assignable(a, ty_with_null(b, 1), 1) && !ty_assignable(b, ty_with_null(a, 1), 1)) {
            /* unrelated interfaces/classes are never equal */
            cerr(n, "incomparable types: %s and %s", ty_str(a), ty_str(b));
        }
        return TBOOL;
    }
    /* & | ^ */
    if (is_bool(a) && is_bool(b)) return TBOOL;
    JType *ua = ty_unbox(a), *ub = ty_unbox(b);
    if (ty_is_integral(ua) && ty_is_integral(ub)) return ty_binary_numeric(ua, ub);
    bad_operands(n, a, b);
    return ty_error();
}

static int is_variable(Node *e) {
    return (e->kind == N_NAME && (e->rk == R_LOCAL || e->rk == R_FIELD || e->rk == R_STATIC_FIELD)) ||
           (e->kind == N_FIELDACC && (e->rk == R_FIELD || e->rk == R_STATIC_FIELD || e->rk == R_THIS_FIELD)) ||
           e->kind == N_INDEX;
}

static void check_final_assign(Ctx *cx, Node *lhs, int compound) {
    if (lhs->kind == N_NAME && lhs->rk == R_LOCAL) {
        JLocal *l = lhs->sym;
        l->assigns++;
        if (l->flags & M_FINAL) {
            if (l->has_init || l->is_param || compound || l->assigns > 1) cerr(lhs, "cannot assign a value to final variable '%s'", l->name);
        }
        if (l->index < cx->lambda_base) cerr(lhs, "local variable '%s' is accessed from a lambda; it must be effectively final", l->name);
        return;
    }
    if ((lhs->kind == N_NAME || lhs->kind == N_FIELDACC) &&
        (lhs->rk == R_FIELD || lhs->rk == R_STATIC_FIELD || lhs->rk == R_THIS_FIELD)) {
        JField *f = lhs->sym;
        if (f->flags & M_FINAL) {
            int ok = !compound && f->cls == cx->cls && (f->decl ? !f->decl->b : 1) &&
                     (((f->flags & M_STATIC) && cx->method == NULL && cx->is_static) ||
                      (!(f->flags & M_STATIC) && cx->in_ctor && (lhs->kind == N_NAME || lhs->a->kind == N_THIS)));
            if (!ok) cerr(lhs, "cannot assign a value to final field '%s'", f->name);
        }
    } else if (lhs->kind == N_FIELDACC && lhs->rk == R_ARRAYLEN) {
        cerr(lhs, "cannot assign a value to final field 'length'");
    }
}

static JType *check_assign(Ctx *cx, Node *n) {
    JType *lt = check_expr(cx, n->a, NULL);
    if (lt->kind == TY_ERROR) { check_expr(cx, n->b, NULL); return ty_error(); }
    if (!is_variable(n->a)) {
        cerr(n->a, "the left-hand side of an assignment must be a variable");
        check_expr(cx, n->b, NULL);
        return ty_error();
    }
    JLocal *l = local_of(n->a);
    JType *declared = l ? l->type : lt; /* narrowing must not weaken the declared type */
    if (n->a->kind == N_NAME && n->a->rk == R_LOCAL) lt = declared;
    int compound = strcmp(n->name, "=") != 0;
    if (!compound) {
        JType *rt = check_expr(cx, n->b, lt);
        check_assignable(cx, n->b, rt, lt, "assignment");
        check_final_assign(cx, n->a, 0);
        if (l) l->nonnull = (declared->nullable && rt->kind != TY_ERROR && rt->kind != TY_NULL && !rt->nullable);
        return lt;
    }
    JType *rt = check_expr(cx, n->b, NULL);
    check_final_assign(cx, n->a, 1);
    if (rt->kind == TY_ERROR) return lt;
    char op[4];
    strncpy(op, n->name, 3);
    op[strlen(n->name) - 1] = 0;
    if (!strcmp(op, "+") && ty_is_string(lt)) return lt;
    JType *ul = ty_unbox(lt), *ur = ty_unbox(rt);
    int ok;
    if (!strcmp(op, "&") || !strcmp(op, "|") || !strcmp(op, "^")) ok = (ul->kind == TY_BOOL && ur->kind == TY_BOOL) || (ty_is_integral(ul) && ty_is_integral(ur));
    else if (!strcmp(op, "<<") || !strcmp(op, ">>")) ok = ty_is_integral(ul) && ty_is_integral(ur);
    else ok = ty_is_numeric(ul) && ty_is_numeric(ur);
    if (!ok) { cerr(n, "operator '%s' cannot be applied to %s and %s", n->name, ty_str(lt), ty_str(rt)); return lt; }
    if (lt->nullable) cerr(n->a, "possible null: nullable %s cannot be used with operator '%s'; check for null first", ty_str(lt), n->name);
    return lt;
}

static JType *check_unary(Ctx *cx, Node *n) {
    const char *op = n->name;
    if (!strcmp(op, "-") && n->a->kind == N_LIT_INT && !strcmp(n->a->name, "2147483648")) {
        n->a->flags = 99;
    }
    JType *t = check_expr(cx, n->a, NULL);
    if (t->kind == TY_ERROR) return t;
    if (!strcmp(op, "!")) {
        if (!is_bool(t)) { cerr(n, "operator '!' cannot be applied to %s", ty_str(t)); return TBOOL; }
        return TBOOL;
    }
    if (!strcmp(op, "++") || !strcmp(op, "--")) {
        JType *u = ty_unbox(t);
        if (!ty_is_numeric(u)) { cerr(n, "operator '%s' cannot be applied to %s", op, ty_str(t)); return t; }
        if (!is_variable(n->a)) cerr(n->a, "operand of '%s' must be a variable", op);
        else check_final_assign(cx, n->a, 1);
        return t;
    }
    JType *u = ty_unbox(t);
    if (!strcmp(op, "~")) {
        if (!ty_is_integral(u)) { cerr(n, "operator '~' cannot be applied to %s", ty_str(t)); return ty_error(); }
        return u->kind == TY_LONG ? u : TINT;
    }
    if (!ty_is_numeric(u)) { cerr(n, "operator '%s' cannot be applied to %s", op, ty_str(t)); return ty_error(); }
    non_null_operand(n, n->a, t);
    return ty_binary_numeric(u, TINT);
}

static JType *common_type(Ctx *cx, Node *at, JType *a, JType *b, JType *expected) {
    (void)cx; (void)at;
    if (a->kind == TY_ERROR) return b;
    if (b->kind == TY_ERROR) return a;
    if (a->kind == TY_NULL && b->kind == TY_NULL) return a;
    if (a->kind == TY_NULL) return ty_is_prim(b) ? ty_with_null(ty_box(b), 1) : ty_with_null(b, 1);
    if (b->kind == TY_NULL) return ty_is_prim(a) ? ty_with_null(ty_box(a), 1) : ty_with_null(a, 1);
    if (ty_is_prim(a) || ty_is_prim(b)) {
        JType *n = ty_binary_numeric(a, b);
        if (n) return n;
        if (ty_unbox(a)->kind == TY_BOOL && ty_unbox(b)->kind == TY_BOOL) return TBOOL;
    }
    if (expected && ty_assignable(a, expected, 1) && ty_assignable(b, expected, 1)) return expected;
    JType *ba = ty_is_prim(a) ? ty_box(a) : a, *bb = ty_is_prim(b) ? ty_box(b) : b;
    int nul = ba->nullable || bb->nullable;
    if (ty_assignable(ba, ty_with_null(bb, 1), 1)) return ty_with_null(bb, nul);
    if (ty_assignable(bb, ty_with_null(ba, 1), 1)) return ty_with_null(ba, nul);
    if (ba->kind == TY_CLASS) {
        for (JClass *c = ba->cls; c; c = c->super ? c->super->cls : NULL) {
            JType *s = ty_as_super(bb, c);
            if (s && c != jt_object()) return ty_with_null(ty_as_super(ba, c), nul);
        }
        /* shared interface */
        jt_complete(ba->cls);
        for (int i = 0; i < ba->cls->ifaces.count; i++) {
            JType *it = ba->cls->ifaces.items[i];
            if (ty_as_super(bb, it->cls)) return ty_with_null(it, nul);
        }
    }
    return ty_with_null(jt_object_type(), nul);
}

static JType *check_cond_expr(Ctx *cx, Node *n, JType *expected) {
    check_cond(cx, n->a);
    JList undo = {0};
    narrow_apply(cx, n->a, 1, &undo);
    JType *a = check_expr(cx, n->b, expected);
    narrow_undo(&undo);
    narrow_apply(cx, n->a, 0, &undo);
    JType *b = check_expr(cx, n->c, expected);
    narrow_undo(&undo);
    if (a->kind == TY_VOID || b->kind == TY_VOID) { cerr(n, "branches of '?:' must produce a value"); return ty_error(); }
    return common_type(cx, n, a, b, expected);
}

static int castable(JType *from, JType *to) {
    if (ty_is_prim(from) && ty_is_prim(to)) {
        return (from->kind == TY_BOOL) == (to->kind == TY_BOOL);
    }
    if (ty_is_prim(from)) return ty_assignable(from, to, 1);
    if (ty_is_prim(to)) {
        JType *u = ty_unbox(from);
        if (ty_is_prim(u)) return (u->kind == TY_BOOL) == (to->kind == TY_BOOL);
        return from->kind == TY_CLASS && (ty_is_object(from) || from->cls == jt_find_class("ash.lang.Number") || from->cls == jt_find_class("ash.lang.Comparable"));
    }
    if (ty_assignable(from, ty_with_null(to, 1), 1) || ty_assignable(to, ty_with_null(from, 1), 1)) return 1;
    if (from->kind == TY_CLASS && to->kind == TY_CLASS) {
        if (from->cls->kind == N_INTERFACE && !(to->cls->flags & M_FINAL) ) return 1;
        if (to->cls->kind == N_INTERFACE && !(from->cls->flags & M_FINAL)) return 1;
    }
    if (from->kind == TY_TVAR || to->kind == TY_TVAR) return 1;
    return 0;
}

static JType *check_cast(Ctx *cx, Node *n) {
    JType *to = jt_resolve_type(n->a, cx->cls, cx->method, 1);
    JType *from = check_expr(cx, n->b, NULL);
    if (to->kind == TY_ERROR || from->kind == TY_ERROR) return to;
    if (from->kind == TY_NULL) {
        if (!to->nullable && ty_is_ref(to)) cerr(n, "cannot cast null to non-null type %s", ty_str(to));
        return to;
    }
    if (from->nullable && !to->nullable && ty_is_ref(to) && !ty_is_prim(ty_unbox(to))) {
        cerr(n, "possible null: casting nullable %s to non-null %s; check for null first", ty_str(from), ty_str(to));
    }
    if (!castable(from, to)) cerr(n, "inconvertible types: cannot cast %s to %s", ty_str(from), ty_str(to));
    return to;
}

static JType *check_instanceof(Ctx *cx, Node *n) {
    JType *e = check_expr(cx, n->a, NULL);
    JType *t = jt_resolve_type(n->b, cx->cls, cx->method, 1);
    if (e->kind != TY_ERROR && t->kind != TY_ERROR) {
        if (ty_is_prim(e)) cerr(n->a, "instanceof needs a reference expression, found %s", ty_str(e));
        else if (ty_is_prim(t)) cerr(n->b, "instanceof needs a reference type, found %s", ty_str(t));
        else if (!castable(e, t)) cerr(n, "incompatible types: %s can never be an instance of %s", ty_str(e), ty_str(t));
    }
    if (n->name) {
        JLocal *l = declare_local(cx, n, n->name, ty_with_null(t, 0), 0, 0);
        l->is_temp = 1;
        l->has_init = 1;
        n->sym = l;
    }
    return TBOOL;
}

static JType *check_array(Ctx *cx, Node *n) {
    JType *base = jt_resolve_type(n->a, cx->cls, cx->method, 1);
    if (base->kind == TY_ERROR) return base;
    /* declared-type form (`int[] a = {..}`) carries its dims in the type node */
    for (int i = 0; i < (n->a->flags & 0xFFFF) && base->kind == TY_ARRAY; i++) base = base->elem;
    JType *t = base;
    for (int i = 0; i < n->flags; i++) t = ty_array(t);
    for (int i = 0; i < n->l1.count; i++) check_coerce(cx, n->l1.items[i], TINT, "array dimension");
    if (n->b) {
        JType *elem = t->elem;
        Node *init = n->b;
        for (int i = 0; i < init->l1.count; i++) {
            Node *e = init->l1.items[i];
            if (e->kind == N_NEWARRAY) {
                JType *it = check_expr(cx, e, elem);
                check_assignable(cx, e, it, elem, "array element");
            } else {
                check_coerce(cx, e, elem, "array element");
            }
        }
    }
    return t;
}

/* ---------------------------------------------------------------- entry point */

JType *check_expr_ex(Ctx *cx, Node *n, JType *expected, int allow_type);

JType *check_expr_ex(Ctx *cx, Node *n, JType *expected, int allow_type) {
    JType *t = NULL;
    switch (n->kind) {
        case N_LIT_INT: {
            unsigned long long v = strtoull(n->name, NULL, 0);
            if (v > 2147483647ULL && !(v == 2147483648ULL && n->flags == 99) && !(n->name[0] == '0' && (n->name[1] == 'x' || n->name[1] == 'X') && v <= 0xFFFFFFFFULL)) {
                cerr(n, "integer number too large: %s (use an 'L' suffix for long)", n->name);
            }
            t = TINT;
            break;
        }
        case N_LIT_LONG: t = ty_prim(TY_LONG); break;
        case N_LIT_FLOAT: t = ty_prim(TY_FLOAT); break;
        case N_LIT_DOUBLE: t = ty_prim(TY_DOUBLE); break;
        case N_LIT_CHAR: t = ty_prim(TY_CHAR); break;
        case N_LIT_STRING: t = jt_string_type(); break;
        case N_LIT_BOOL: t = TBOOL; break;
        case N_LIT_NULL: t = ty_null(); break;
        case N_NAME: t = check_name(cx, n, allow_type); break;
        case N_FIELDACC: t = check_fieldacc(cx, n, allow_type); break;
        case N_THIS:
            if (cx->is_static) { cerr(n, "'this' cannot be used in a static context"); t = ty_error(); }
            else t = cx->cls->self_type;
            break;
        case N_SUPER:
            cerr(n, "'super' can only be used as a method or constructor call target");
            t = ty_error();
            break;
        case N_CALL: t = check_call(cx, n, expected); break;
        case N_NEW: t = check_new_object(cx, n, expected); break;
        case N_NEWARRAY: case N_ARRAYINIT: t = check_array(cx, n); break;
        case N_INDEX: {
            JType *a = check_expr(cx, n->a, NULL);
            check_coerce(cx, n->b, TINT, "array index");
            if (a->kind == TY_ERROR) { t = a; break; }
            deref_check(n, n->a, a);
            if (a->kind != TY_ARRAY) { cerr(n, "indexing requires an array, found %s", ty_str(a)); t = ty_error(); }
            else t = a->elem;
            break;
        }
        case N_UNARY: t = check_unary(cx, n); break;
        case N_POSTFIX: {
            JType *a = check_expr(cx, n->a, NULL);
            JType *u = ty_unbox(a);
            if (a->kind != TY_ERROR && !ty_is_numeric(u)) cerr(n, "operator '%s' cannot be applied to %s", n->name, ty_str(a));
            else if (a->kind != TY_ERROR) {
                if (!is_variable(n->a)) cerr(n->a, "operand of '%s' must be a variable", n->name);
                else check_final_assign(cx, n->a, 1);
            }
            t = a;
            break;
        }
        case N_BINARY: t = check_binary(cx, n); break;
        case N_ASSIGN: t = check_assign(cx, n); break;
        case N_COND: t = check_cond_expr(cx, n, expected); break;
        case N_CAST: t = check_cast(cx, n); break;
        case N_INSTANCEOF: t = check_instanceof(cx, n); break;
        case N_LAMBDA: case N_METHODREF:
            t = check_lambda(cx, n, expected, NULL);
            break;
        case N_SWITCH: t = check_switch(cx, n, expected); break;
        default:
            cerr(n, "unexpected expression");
            t = ty_error();
    }
    n->type = t;
    return t;
}

JType *check_expr(Ctx *cx, Node *n, JType *expected) {
    JType *t = check_expr_ex(cx, n, expected, 0);
    if (n->rk == R_CLASS || n->rk == R_PACKAGE) return n->type;
    return t;
}
