#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jcommon/H/jcommon.h"
#include "jown/H/region.h"
#include "jtypes/H/check.h"
#include "jtypes/H/jtypes.h"

/* Loop regions: which loops let their body's allocations die with the iteration.
 *
 * `kiln` allocates from a bump heap, so the heap pointer can be moved back to where it was when an iteration
 * began, giving everything the iteration allocated back at once. That is only sound when nothing allocated in the
 * body can be reached after the iteration. The rule here is deliberately narrow, so that this follows from reading
 * the body and nothing else:
 *
 *   - no call into project code (a callee may keep an argument or allocate into a field), no constructor of a
 *     project class, no lambda, `try`, `switch`, `throw` or for-each;
 *   - the calls that remain are the ones that cannot keep anything: String methods that read, `Math`, `print` /
 *     `println` of primitives and Strings, and, on a list declared in the body, the ArrayList methods;
 *   - a reference stored anywhere (a variable, an array element, a list) must go to a variable declared in the
 *     body (whose value is a fresh array / list, or any String), never to a variable declared outside the loop,
 *     a field or an element of an outer array; only primitives may leave the iteration;
 *   - a local of the body that is an array or list starts as a fresh one and is only ever assigned fresh ones, so
 *     it cannot alias something older (storing into it or growing it is then local to the iteration).
 * `return` and `break` leave the loop without a reset (the memory just stays allocated), so they are fine. */

#define MAX_INNER 256

typedef struct {
    JLocal *inner[MAX_INNER];
    int n;
    int overflow;
    int callee;          /* analysing the body of a method called from a region loop: `return` must hand back a primitive */
} Rg;

static int is_prim(JType *t) {
    return t && !t->nullable && t->kind >= TY_BOOL && t->kind <= TY_DOUBLE;
}
static int is_str(JType *t) { return t && t->kind == TY_CLASS && ty_is_string(t); }
static int is_value(JType *t) { return is_prim(t) || is_str(t); }

/* a primitive, or the box of one (`total += list.get(i)`): a number either way */
static int prim_or_box(JType *t) { return t && is_prim(ty_unbox(t)); }

static int list_class(JClass *c) {
    return c && c->qname && (!strcmp(c->qname, "ash.util.ArrayList") || !strcmp(c->qname, "ash.util.List"));
}

static int list_t(JType *t) { return t && !t->nullable && t->kind == TY_CLASS && list_class(t->cls); }
static int array_t(JType *t) { return t && !t->nullable && t->kind == TY_ARRAY; }

static int is_inner(Rg *g, JLocal *l) {
    for (int i = 0; i < g->n; i++) if (g->inner[i] == l) return 1;
    return 0;
}

static void collect(Rg *g, Node *n) {
    if (!n) return;
    /* a for-each variable of a reference type aliases an element that was there before the iteration, so it is
     * never one of the body's fresh locals (a String is a value: assigning to it keeps nothing) */
    if ((n->kind == N_LOCAL || (n->kind == N_FOREACH && n->sym && is_value(((JLocal *)n->sym)->type))) && n->sym) {
        if (g->n < MAX_INNER) g->inner[g->n++] = n->sym; else g->overflow = 1;
    }
    collect(g, n->a); collect(g, n->b); collect(g, n->c);
    JList *ls[] = {&n->l1, &n->l2, &n->l3, &n->l4, &n->l5};
    for (int k = 0; k < 5; k++) for (int i = 0; i < ls[k]->count; i++) collect(g, ls[k]->items[i]);
}

static int expr_ok2(Rg *g, Node *e);
static int stmt_ok2(Rg *g, Node *s);
static Node *why;                                 /* innermost node that failed, for ASH_REGION_TRACE=2 */
static int expr_ok(Rg *g, Node *e) { int r = expr_ok2(g, e); if (!r && !why) why = e; return r; }
static int stmt_ok(Rg *g, Node *s) { int r = stmt_ok2(g, s); if (!r && !why && s) why = s; return r; }

static int fresh_arraylist(Node *e) {
    JMethod *m = e && e->kind == N_NEW ? e->sym : NULL;
    return m && !e->b && e->l1.count == 0 && m->cls && m->cls->qname && !strcmp(m->cls->qname, "ash.util.ArrayList");
}

/* a new array or list that nothing else references */
static int fresh(Rg *g, Node *e) {
    if (!e) return 0;
    if (e->kind == N_ARRAYINIT) return expr_ok(g, e);
    if (e->kind == N_NEWARRAY) return expr_ok(g, e);
    return fresh_arraylist(e);
}

static int args_ok(Rg *g, JList *l) {
    for (int i = 0; i < l->count; i++) if (!expr_ok(g, l->items[i])) return 0;
    return 1;
}

static int name_in(Rg *g, Node *e) {
    return e && e->kind == N_NAME && e->rk == R_LOCAL && e->sym && is_inner(g, e->sym);
}

static int region_pure_callee(JMethod *m);

/* the project's own classes (a standard-library class keeps going through the lists of harmless calls below) */
static int project_cls(JClass *c) {
    return c && c->mem_on && c->qname && strncmp(c->qname, "ash.", 4) != 0;
}

/* a name that stands for a class or a package (`Util.f(x)`, `pkg.Util.f(x)`) evaluates to nothing */
static int type_name(Node *e) {
    return e && (e->kind == N_NAME || e->kind == N_FIELDACC) && (e->rk == R_CLASS || e->rk == R_PACKAGE);
}

static int call_ok(Rg *g, Node *e) {
    JMethod *m = e->sym;
    if (!m || e->flags == 1 || !m->cls || !m->cls->qname) return 0;
    const char *q = m->cls->qname, *nm = m->name;
    int nargs = e->l1.count;
    /* every argument must be a primitive or a String: anything else could be an object whose toString or
     * equals is project code, or a container the callee could keep */
    for (int i = 0; i < nargs; i++) if (!is_value(((Node *)e->l1.items[i])->type)) return 0;
    if (!args_ok(g, &e->l1)) return 0;
    /* project code that provably keeps nothing and answers with a primitive: its allocations die with ours */
    if (project_cls(m->cls) && !m->tmpl) {
        if (!region_pure_callee(m)) return 0;
        if (e->a && !type_name(e->a) && !expr_ok(g, e->a)) return 0;
        return 1;
    }
    /* "x".equals(y) resolves to Object.equals; a String receiver is final, so it is String.equals */
    if (!strcmp(nm, "equals") && nargs == 1 && e->a && is_str(e->a->type)) return expr_ok(g, e->a);
    if (m->cls == jt_string()) {
        static const char *const ok[] = {"length", "charAt", "equals", "isEmpty", "indexOf", "substring", "startsWith", "endsWith", "contains", "compareTo", NULL};
        for (int i = 0; ok[i]; i++) if (!strcmp(nm, ok[i])) return e->a && is_str(e->a->type) && expr_ok(g, e->a);
        return 0;
    }
    if (!strcmp(q, "ash.lang.Math")) return (m->flags & M_STATIC) != 0;
    if (!strcmp(q, "ash.lang.PrintStream")) return (!strcmp(nm, "println") || !strcmp(nm, "print")) && e->a && expr_ok(g, e->a);
    /* only a list declared in the body: it is exactly an ArrayList (nothing else implements List here) and
     * its elements are never compared with project code (contains, equals) */
    if (list_class(m->cls) && name_in(g, e->a)) {
        if (!strcmp(nm, "size") || !strcmp(nm, "isEmpty") || !strcmp(nm, "removeLast")) return nargs == 0;
        if (!strcmp(nm, "get") || !strcmp(nm, "remove")) return nargs == 1;
        if (!strcmp(nm, "add")) return nargs == 1 || nargs == 2;
        if (!strcmp(nm, "set")) return nargs == 2;
        return 0;
    }
    return 0;
}

/* may `target op= rhs` be done: only primitives leave the iteration */
static int assign_ok(Rg *g, Node *e) {
    Node *t = e->a;
    int plain = !strcmp(e->name, "=");
    if (t->kind == N_NAME && t->rk == R_LOCAL && t->sym) {
        JLocal *l = t->sym;
        if (is_prim(l->type)) return prim_or_box(e->b->type);
        if (!is_inner(g, l)) return 0;
        if (is_str(l->type)) return is_value(e->b->type);
        if ((array_t(l->type) || list_t(l->type)) && plain) return fresh(g, e->b);
        return 0;
    }
    if (t->kind == N_INDEX) {
        if (is_prim(t->type)) return prim_or_box(e->b->type);   /* a number into an array of numbers: nothing to keep */
        return name_in(g, t->a) && (is_value(e->b->type) || fresh(g, e->b)) && plain;
    }
    if ((t->kind == N_NAME || t->kind == N_FIELDACC) && t->sym && (t->rk == R_FIELD || t->rk == R_STATIC_FIELD || t->rk == R_THIS_FIELD))
        return is_prim(t->type) && prim_or_box(e->b->type);
    return 0;
}

static int expr_ok2(Rg *g, Node *e) {
    if (!e) return 1;
    switch (e->kind) {
        case N_LIT_INT: case N_LIT_LONG: case N_LIT_FLOAT: case N_LIT_DOUBLE: case N_LIT_CHAR: case N_LIT_STRING:
        case N_LIT_BOOL: case N_LIT_NULL: case N_THIS:
            return 1;
        case N_NAME:
            return e->rk == R_LOCAL || e->rk == R_FIELD || e->rk == R_STATIC_FIELD || e->rk == R_THIS_FIELD || e->rk == R_CLASS || e->rk == R_PACKAGE;
        case N_FIELDACC:
            return (e->rk == R_ARRAYLEN || e->rk == R_FIELD || e->rk == R_STATIC_FIELD || e->rk == R_THIS_FIELD) && expr_ok(g, e->a);
        case N_INDEX:
            return expr_ok(g, e->a) && expr_ok(g, e->b);
        case N_UNARY:
            if (e->name[0] == '&') return 0;
            if ((e->name[0] == '+' && e->name[1] == '+') || (e->name[0] == '-' && e->name[1] == '-')) {
                Node as = *e;                                   /* x++ is x += 1 as far as keeping a value goes */
                Node one = {0};
                one.type = e->a->type;
                as.a = e->a; as.b = &one; as.name = "+=";
                return is_prim(e->a->type) && assign_ok(g, &as) && expr_ok(g, e->a);
            }
            return expr_ok(g, e->a);
        case N_POSTFIX: {
            Node as = *e;
            Node one = {0};
            one.type = e->a->type;
            as.a = e->a; as.b = &one; as.name = "+=";
            return is_prim(e->a->type) && assign_ok(g, &as) && expr_ok(g, e->a);
        }
        case N_BINARY:
            /* `+` on Strings calls toString of objects (project code): both sides must be values */
            if (!strcmp(e->name, "+") && (!is_value(e->a->type) || !is_value(e->b->type))) return 0;
            return expr_ok(g, e->a) && expr_ok(g, e->b);
        case N_COND:
            return expr_ok(g, e->a) && expr_ok(g, e->b) && expr_ok(g, e->c);
        case N_CAST:
            return is_prim(e->type) && expr_ok(g, e->b);
        case N_ASSIGN:
            return assign_ok(g, e) && expr_ok(g, e->a) && expr_ok(g, e->b);
        case N_NEWARRAY:
            if (!args_ok(g, &e->l1)) return 0;
            return !e->b || expr_ok(g, e->b);
        case N_ARRAYINIT:
            return args_ok(g, &e->l1);
        case N_NEW:
            return fresh_arraylist(e);
        case N_CALL:
            return call_ok(g, e);
        default:
            return 0;
    }
}

/* for-each over an array or an ArrayList: the loop reads the data pointer and the count once, before the first
 * iteration, and nothing else of the iteration lives on the heap. Any other Iterable calls project code. */
static int native_iter_t(JType *t) { return array_t(t) || list_t(t); }

static int foreach_ok(Rg *g, Node *s) {
    JType *it = s->b ? s->b->type : NULL;
    if (!s->sym || !native_iter_t(it)) return 0;
    /* a list that the body itself grows would leave the loop with a stale data pointer */
    if (list_t(it) && name_in(g, s->b)) return 0;
    return expr_ok(g, s->b) && stmt_ok(g, s->c);
}

static int stmts_ok(Rg *g, JList *l) {
    for (int i = 0; i < l->count; i++) if (!stmt_ok(g, l->items[i])) return 0;
    return 1;
}

static int stmt_ok2(Rg *g, Node *s) {
    if (!s) return 1;
    switch (s->kind) {
        case N_BLOCK: case N_SEQ: return stmts_ok(g, &s->l1);
        case N_EMPTY: case N_BREAK: case N_CONTINUE: return 1;
        case N_LOCAL: {
            JLocal *l = s->sym;
            if (!l) return 0;
            if (is_value(l->type)) return !s->b || expr_ok(g, s->b);
            if ((array_t(l->type) || list_t(l->type)) && s->b) return fresh(g, s->b);
            return 0;
        }
        case N_EXPRSTMT: return expr_ok(g, s->a);
        case N_IF: return expr_ok(g, s->a) && stmt_ok(g, s->b) && stmt_ok(g, s->c);
        case N_WHILE: case N_DO: return expr_ok(g, s->a) && stmt_ok(g, s->b);
        case N_FOR: {
            if (!stmts_ok(g, &s->l1) || !expr_ok(g, s->a)) return 0;
            for (int i = 0; i < s->l2.count; i++) if (!expr_ok(g, s->l2.items[i])) return 0;
            return stmt_ok(g, s->b);
        }
        case N_FOREACH: return foreach_ok(g, s);
        case N_RETURN:
            if (g->callee) return s->a && is_prim(s->a->type) && expr_ok(g, s->a);
            return expr_ok(g, s->a) && (!s->a || is_value(s->a->type));
        default: return 0;
    }
}

/* the body is also walked for statements that merely have to exist; a body with no allocation at all gains
 * nothing from a region, so require at least one allocating construct */
static int allocates(Node *n);
static int callee_allocates(JMethod *m) {
    if (!m || m->rg_pure != 1 || !m->decl) return 0;
    return allocates(m->decl->b);
}

static int allocates(Node *n) {
    if (!n) return 0;
    if (n->kind == N_CALL && n->sym && ((JMethod *)n->sym)->rg_pure == 1 && callee_allocates(n->sym)) return 1;
    if (n->kind == N_NEWARRAY || n->kind == N_ARRAYINIT || n->kind == N_NEW) return 1;
    if (n->kind == N_BINARY && !strcmp(n->name, "+") && is_str(n->type)) return 1;
    if (n->kind == N_CALL && n->sym && ((JMethod *)n->sym)->cls == jt_string() && !strcmp(((JMethod *)n->sym)->name, "substring")) return 1;
    if (n->kind == N_ASSIGN && n->name[0] == '+' && is_str(n->a->type)) return 1;
    if (allocates(n->a) || allocates(n->b) || allocates(n->c)) return 1;
    JList *ls[] = {&n->l1, &n->l2, &n->l3, &n->l4, &n->l5};
    for (int k = 0; k < 5; k++) for (int i = 0; i < ls[k]->count; i++) if (allocates(ls[k]->items[i])) return 1;
    return 0;
}

/* A call to project code may be made inside a region loop when the callee is fixed at compile time (static,
 * private or final), takes and returns only primitives and Strings, and keeps nothing it allocates: its body must
 * satisfy the rules of a loop body (every reference stored goes to a variable of the callee itself, no
 * constructor of a project class, no lambda / try / throw / switch) and every call in it must in turn be such a
 * method or one of the harmless library calls. A recursive cycle is never accepted. Whatever the callee
 * allocates is then unreachable once it has returned, so the region of the calling iteration reclaims it. */
static int region_pure_callee(JMethod *m) {
    if (m->rg_pure == 1) return 1;
    if (m->rg_pure >= 2) return 0;                    /* no, or a recursive cycle */
    m->rg_pure = 2;
    JClass *c = m->cls;
    if (!project_cls(c)) return 0;
    if (c->kind != N_CLASS && c->kind != N_RECORD) return 0;
    if (m->tmpl || m->is_ctor || m->synthetic || !m->decl || !m->decl->b || m->decl->b->kind != N_BLOCK) return 0;
    if ((m->flags & (M_VARARGS | M_ABSTRACT | M_NATIVE)) || !(m->flags & (M_STATIC | M_PRIVATE | M_FINAL))) return 0;
    if (m->tparams.count || !is_prim(m->ret)) return 0;
    if (m->decl->l1.count != m->params.count) return 0;
    Rg *g = j_alloc(sizeof(Rg));
    g->callee = 1;
    for (int i = 0; i < m->params.count; i++) {
        if (!is_value(((JParam *)m->params.items[i])->type)) return 0;
        Node *pn = m->decl->l1.items[i];
        JLocal *l = pn->kind == N_PARAM ? pn->sym : NULL;
        if (!l) return 0;
        if (g->n < MAX_INNER) g->inner[g->n++] = l; else return 0;
    }
    collect(g, m->decl->b);
    if (g->overflow) return 0;
    m->rg_pure = 3;
    int ok = stmt_ok(g, m->decl->b);
    m->rg_pure = ok ? 1 : 2;
    return ok;
}

int region_loop_ok(Node *loop) {
    if (loop->kind != N_WHILE && loop->kind != N_DO && loop->kind != N_FOR && loop->kind != N_FOREACH) return 0;
    Rg *g = j_alloc(sizeof(Rg));
    Node *body = loop->kind == N_FOREACH ? loop->c : loop->b;
    why = NULL;
    int ok;
    if (loop->kind == N_FOREACH) {
        /* the iterable is evaluated once before the first iteration, i.e. outside the region: it may be any
         * expression, as long as the loop itself is over an array or an ArrayList */
        collect(g, loop);
        if (g->overflow || !loop->sym || !native_iter_t(loop->b ? loop->b->type : NULL)) return 0;
        ok = stmt_ok(g, body) && allocates(body);
    } else {
        collect(g, body);
        if (g->overflow) return 0;
        ok = stmt_ok(g, body) && allocates(body);
    }
    if (!ok && getenv("ASH_REGION_TRACE") && getenv("ASH_REGION_TRACE")[0] == '2' && loop->line)
        fprintf(stderr, "no region: %s:%d (stopped at %s:%d kind %d)\n", loop->path ? loop->path : "?", loop->line, why && why->path ? why->path : "?", why ? why->line : 0, why ? (int)why->kind : -1);
    return ok;
}

static void mark(Node *n) {
    if (!n) return;
    if ((n->kind == N_WHILE || n->kind == N_DO || n->kind == N_FOR || n->kind == N_FOREACH) && region_loop_ok(n)) n->flags |= JOWN_REGION_LOOP;
    mark(n->a); mark(n->b); mark(n->c);
    JList *ls[] = {&n->l1, &n->l2, &n->l3, &n->l4, &n->l5};
    for (int k = 0; k < 5; k++) for (int i = 0; i < ls[k]->count; i++) mark(ls[k]->items[i]);
}

void region_mark_loops(Node *body) { mark(body); }
