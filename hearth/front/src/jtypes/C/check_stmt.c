#include <stdlib.h>
#include <string.h>

#include "jtypes/H/check.h"

/* Statement checking: locals, control flow, exceptions, switch, reachability. */

#define TBOOL ty_prim(TY_BOOL)

/* ---------------------------------------------------------------- reachability */

static int has_break(Node *n, int depth) {
    if (!n) return 0;
    switch (n->kind) {
        case N_BREAK: return depth == 0;
        case N_BLOCK: case N_SEQ:
            for (int i = 0; i < n->l1.count; i++) if (has_break(n->l1.items[i], depth)) return 1;
            return 0;
        case N_IF: return has_break(n->b, depth) || has_break(n->c, depth);
        case N_WHILE: case N_DO: case N_FOR: return has_break(n->b, depth + 1);
        case N_FOREACH: return has_break(n->c, depth + 1);
        case N_SWITCH:
            for (int i = 0; i < n->l1.count; i++) {
                Node *c = n->l1.items[i];
                if (has_break(c->b, depth + 1)) return 1;
                for (int j = 0; j < c->l2.count; j++) if (has_break(c->l2.items[j], depth + 1)) return 1;
            }
            return 0;
        case N_TRY:
            if (has_break(n->b, depth) || has_break(n->c, depth)) return 1;
            for (int i = 0; i < n->l1.count; i++) if (has_break(((Node *)n->l1.items[i])->b, depth)) return 1;
            return 0;
        default: return 0;
    }
}

/* A constant expression of type bool (JLS 15.29): literals, `!`, `&&`, `||`, `==` / `!=` on bools and the six
 * comparisons of int constants, and final variables initialised with such an expression. Returns 1 and sets *out
 * when it is one. Only these are used for reachability, so a loop whose condition is one of them is infinite
 * (or never entered) exactly as javac decides. */
/* only simple names and Type.NAME may stand for a constant variable (`obj.N` is not a constant expression) */
static int const_shape(Node *e) {
    if (!e) return 0;
    switch (e->kind) {
        case N_LIT_INT: case N_NAME: return 1;
        case N_FIELDACC: return e->rk == R_STATIC_FIELD && e->a && e->a->rk == R_CLASS;
        case N_UNARY: return (!strcmp(e->name, "-") || !strcmp(e->name, "+")) && const_shape(e->a);
        case N_BINARY: return e->name[1] == 0 && strchr("+-*/%", e->name[0]) && const_shape(e->a) && const_shape(e->b);
        default: return 0;
    }
}

static int const_bool(Node *e, int *out, int depth) {
    if (!e || depth > 16) return 0;
    if (e->kind == N_LIT_BOOL) { *out = !strcmp(e->name, "true"); return 1; }
    if ((e->kind == N_NAME || (e->kind == N_FIELDACC && e->rk == R_STATIC_FIELD && e->a && e->a->rk == R_CLASS)) && e->sym &&
        e->type && e->type->kind == TY_BOOL && !e->type->nullable) {
        Node *init = NULL;
        if (e->rk == R_STATIC_FIELD || e->rk == R_FIELD || e->rk == R_THIS_FIELD) {
            JField *f = e->sym;
            if ((f->flags & M_FINAL) && f->decl) init = f->decl->b;
        } else if (e->rk == R_LOCAL) {
            JLocal *l = e->sym;
            if ((l->flags & M_FINAL) && l->decl && l->decl->kind == N_LOCAL) init = l->decl->b;
        }
        return init && const_bool(init, out, depth + 1);
    }
    if (e->kind == N_UNARY && !strcmp(e->name, "!")) {
        int v;
        if (!const_bool(e->a, &v, depth + 1)) return 0;
        *out = !v;
        return 1;
    }
    if (e->kind == N_BINARY) {
        const char *op = e->name;
        int a, b;
        if (!strcmp(op, "&&") || !strcmp(op, "||") || (e->a->type && e->a->type->kind == TY_BOOL && (!strcmp(op, "==") || !strcmp(op, "!=")))) {
            if (!const_bool(e->a, &a, depth + 1) || !const_bool(e->b, &b, depth + 1)) return 0;
            *out = op[0] == '&' ? (a && b) : op[0] == '|' ? (a || b) : op[0] == '=' ? (a == b) : (a != b);
            return 1;
        }
        long long x, y;
        if (e->a->type && e->b->type && e->a->type->kind == TY_INT && e->b->type->kind == TY_INT &&
            (!strcmp(op, "<") || !strcmp(op, "<=") || !strcmp(op, ">") || !strcmp(op, ">=") || !strcmp(op, "==") || !strcmp(op, "!=")) &&
            const_shape(e->a) && const_shape(e->b) && jt_const_int(e->a, &x) && jt_const_int(e->b, &y)) {
            *out = !strcmp(op, "<") ? x < y : !strcmp(op, "<=") ? x <= y : !strcmp(op, ">") ? x > y : !strcmp(op, ">=") ? x >= y : !strcmp(op, "==") ? x == y : x != y;
            return 1;
        }
    }
    return 0;
}

static int always_true(Node *c) {
    int v;
    return !c || (const_bool(c, &v, 0) && v);
}

int can_complete(Node *n) {
    if (!n) return 1;
    switch (n->kind) {
        case N_RETURN: case N_THROW: case N_BREAK: case N_CONTINUE: return 0;
        case N_BLOCK: case N_SEQ:
            for (int i = 0; i < n->l1.count; i++) if (!can_complete(n->l1.items[i])) return 0;
            return 1;
        case N_IF:
            return !n->c || can_complete(n->b) || can_complete(n->c);
        case N_WHILE: return !always_true(n->a) || has_break(n->b, 0);
        case N_DO: return (!always_true(n->a) && can_complete(n->b)) || has_break(n->b, 0) || (!always_true(n->a));
        case N_FOR: return !always_true(n->a) || has_break(n->b, 0);
        case N_SWITCH: {
            int has_default = 0;
            for (int i = 0; i < n->l1.count; i++) if (((Node *)n->l1.items[i])->flags & 1) has_default = 1;
            if (!has_default) return 1;
            for (int i = 0; i < n->l1.count; i++) {
                Node *c = n->l1.items[i];
                if (c->str) {
                    if (c->b->kind == N_BLOCK ? can_complete(c->b) : c->b->kind != N_THROW) return 1;
                    if (has_break(c->b, 0)) return 1;
                } else {
                    for (int j = 0; j < c->l2.count; j++) if (has_break(c->l2.items[j], 0)) return 1;
                    if (i == n->l1.count - 1) {
                        for (int j = 0; j < c->l2.count; j++) if (!can_complete(c->l2.items[j])) return 0;
                        return 1;
                    }
                }
            }
            return 0;
        }
        case N_TRY: {
            if (n->c && !can_complete(n->c)) return 0;
            if (can_complete(n->b)) return 1;
            for (int i = 0; i < n->l1.count; i++) if (can_complete(((Node *)n->l1.items[i])->b)) return 1;
            return 0;
        }
        default: return 1;
    }
}

/* ---------------------------------------------------------------- blocks */

static void check_stmts(Ctx *cx, JList *l) {
    int dead = 0;
    for (int i = 0; i < l->count; i++) {
        Node *s = l->items[i];
        if (dead && s->kind != N_EMPTY) {
            cerr(s, "unreachable statement");
            dead = 0;
        }
        check_stmt(cx, s);
        if (!can_complete(s)) dead = 1;
    }
}

void check_block_body(Ctx *cx, Node *block) {
    int mark;
    push_scope(cx, &mark);
    check_stmts(cx, &block->l1);
    pop_scope(cx, mark);
}

static void check_local(Ctx *cx, Node *n) {
    JType *t = NULL;
    JType *it = NULL;
    if (n->a) {
        t = jt_resolve_type(n->a, cx->cls, cx->method, 1);
        if (t->kind == TY_VOID) { cerr(n, "variable '%s' cannot have type void", n->name); t = ty_error(); }
        if (n->b) {
            it = check_expr(cx, n->b, t);
            check_assignable(cx, n->b, it, t, "initializer");
        }
    } else {
        if (!n->b) { cerr(n, "cannot infer the type of '%s' without an initializer", n->name); t = ty_error(); }
        else {
            it = check_expr(cx, n->b, NULL);
            if (it->kind == TY_NULL) { cerr(n, "cannot infer the type of '%s' from null; declare it explicitly", n->name); t = ty_error(); }
            else if (it->kind == TY_VOID) { cerr(n, "cannot infer the type of '%s' from a void expression", n->name); t = ty_error(); }
            else t = it;
        }
    }
    n->type = t;
    JLocal *l = jt_declare_local(cx, n, n->name, t, n->flags, 0);
    l->has_init = n->b != NULL;
    n->sym = l;
    n->rk = R_LOCAL;
    if (n->b && t->nullable && it && it->kind != TY_NULL && it->kind != TY_ERROR && !it->nullable) l->nonnull = 1;
}

/* ---------------------------------------------------------------- switch */

static int enum_covered(JClass *e, JList *labels) {
    for (int i = 0; i < e->enum_names.count; i++) {
        int found = 0;
        for (int j = 0; j < labels->count && !found; j++) if (!strcmp(labels->items[j], e->enum_names.items[i])) found = 1;
        if (!found) return 0;
    }
    return 1;
}

static int type_covered(JClass *c, JList *types) {
    for (int i = 0; i < types->count; i++) if (ty_is_subclass(c, (JClass *)types->items[i])) return 1;
    jt_complete(c);
    if (c->flags & M_SEALED) {
        if (!c->permits.count && !c->subclasses.count) return 0;
        JList *subs = c->permits.count ? &c->permits : &c->subclasses;
        for (int i = 0; i < subs->count; i++) if (!type_covered(subs->items[i], types)) return 0;
        return subs->count > 0;
    }
    if ((c->flags & M_ABSTRACT) && c->kind == N_CLASS) return 0;
    return 0;
}

JType *check_switch(Ctx *cx, Node *n, JType *expected) {
    int is_expr = n->flags & 1;
    JType *sel = check_expr(cx, n->a, NULL);
    JClass *enum_cls = NULL;
    JType *su = ty_unbox(sel);
    int string_sel = ty_is_string(sel);
    if (sel->kind == TY_CLASS) {
        jt_complete(sel->cls);
        if (sel->cls->kind == N_ENUM) enum_cls = sel->cls;
    }
    if (sel->kind != TY_ERROR) {
        int ok = enum_cls || string_sel || ty_is_integral(su) || su->kind == TY_CHAR || sel->kind == TY_CLASS;
        if (!ok) cerr(n->a, "cannot switch on a value of type %s", ty_str(sel));
        if (sel->nullable) {
            int has_null = 0;
            for (int i = 0; i < n->l1.count; i++) {
                Node *c = n->l1.items[i];
                for (int j = 0; j < c->l1.count; j++) if (((Node *)c->l1.items[j])->kind == N_LIT_NULL) has_null = 1;
            }
            if (!has_null) cerr(n->a, "possible null: switching on nullable %s needs a 'case null'", ty_str(sel));
        }
    }
    JList enum_seen = {0}, type_seen = {0}, consts = {0};
    int has_default = 0;
    JType *result = NULL;
    int mark_outer;
    push_scope(cx, &mark_outer);
    cx->switch_depth++;
    for (int i = 0; i < n->l1.count; i++) {
        Node *c = n->l1.items[i];
        int mark;
        push_scope(cx, &mark);
        JList undo = {0};
        if (c->flags & 1) {
            if (has_default) cerr(c, "duplicate 'default' label");
            has_default = 1;
        }
        for (int j = 0; j < c->l1.count; j++) {
            Node *lb = c->l1.items[j];
            if (lb->kind == N_TYPEPATTERN) {
                JType *pt = jt_resolve_type(lb->a, cx->cls, cx->method, 1);
                lb->type = pt;
                if (pt->kind == TY_CLASS) {
                    if (sel->kind == TY_CLASS && !ty_assignable(pt, ty_with_null(sel, 1), 1) && !ty_assignable(sel, ty_with_null(pt, 1), 1) &&
                        pt->cls->kind != N_INTERFACE && sel->cls->kind != N_INTERFACE) {
                        cerr(lb, "incompatible types: %s can never be %s", ty_str(sel), ty_str(pt));
                    }
                    for (int k = 0; k < type_seen.count; k++) {
                        if (ty_is_subclass(pt->cls, (JClass *)type_seen.items[k])) cerr(lb, "this case is dominated by an earlier case");
                    }
                    j_list_push(&type_seen, pt->cls);
                }
                JLocal *l = jt_declare_local(cx, lb, lb->name, ty_with_null(pt, 0), 0, 0);
                l->has_init = 1;
                lb->sym = l;
            } else if (lb->kind == N_LIT_NULL) {
                lb->type = ty_null();
            } else if (enum_cls && lb->kind == N_NAME) {
                JField *f = jt_find_field(enum_cls, lb->name);
                if (!f || !f->is_enum_const) cerr(lb, "'%s' is not a constant of enum %s", lb->name, enum_cls->name);
                else {
                    for (int k = 0; k < enum_seen.count; k++) if (!strcmp(enum_seen.items[k], lb->name)) cerr(lb, "duplicate case label '%s'", lb->name);
                    j_list_push(&enum_seen, (void *)lb->name);
                    lb->rk = R_ENUM_CONST;
                    lb->sym = f;
                    lb->type = f->type;
                }
            } else {
                JType *lt = check_expr(cx, lb, sel);
                if (sel->kind != TY_ERROR && lt->kind != TY_ERROR) {
                    JType *target = enum_cls || sel->kind != TY_CLASS ? sel : su;
                    if (!ty_assignable(lt, ty_with_null(target, 1), 1) && !(ty_is_prim(lt) && ty_is_prim(su) && const_int_value(lb, &(long){0}))) {
                        cerr(lb, "case label of type %s does not match switch type %s", ty_str(lt), ty_str(sel));
                    }
                }
                const char *key = lb->kind == N_LIT_STRING || lb->kind == N_LIT_INT || lb->kind == N_LIT_CHAR ? lb->name : NULL;
                if (key) {
                    for (int k = 0; k < consts.count; k++) if (!strcmp(consts.items[k], key)) cerr(lb, "duplicate case label '%s'", key);
                    j_list_push(&consts, (void *)key);
                } else if (lb->kind != N_NAME && lb->kind != N_FIELDACC) {
                    cerr(lb, "case labels must be constants");
                }
            }
            if (lb->kind == N_TYPEPATTERN) { /* the binding is already non-null */ }
        }
        if (c->str) { /* arrow form */
            Node *b = c->b;
            if (b->kind == N_EXPRSTMT) {
                JType *t;
                if (is_expr) {
                    t = check_expr(cx, b->a, expected);
                    if (t->kind == TY_VOID) cerr(b->a, "switch expression arms must produce a value");
                    else if (t->kind != TY_ERROR) {
                        if (!result || result->kind == TY_ERROR) result = t;
                        else {
                            JType *bu = ty_unbox(result), *tu = ty_unbox(t);
                            if (ty_is_numeric(bu) && ty_is_numeric(tu)) result = ty_binary_numeric(bu, tu);
                            else if (t->kind == TY_NULL) result = ty_with_null(result, 1);
                            else if (result->kind == TY_NULL) result = ty_with_null(ty_is_prim(t) ? ty_box(t) : t, 1);
                            else if (ty_assignable(result, ty_with_null(t, 1), 1)) result = ty_with_null(t, result->nullable || t->nullable);
                            else if (!ty_assignable(t, ty_with_null(result, 1), 1)) result = expected ? expected : jt_object_type();
                            else result = ty_with_null(result, result->nullable || t->nullable);
                        }
                    }
                } else {
                    t = check_expr(cx, b->a, NULL);
                }
            } else if (b->kind == N_BLOCK) {
                if (is_expr) cerr(b, "switch expression arms must be a single expression or a throw");
                check_stmt(cx, b);
            } else {
                check_stmt(cx, b);
            }
        } else {
            if (is_expr) cerr(c, "switch expressions use '->' arms, not ':'");
            check_stmts(cx, &c->l2);
        }
        narrow_undo(&undo);
        pop_scope(cx, mark);
    }
    cx->switch_depth--;
    pop_scope(cx, mark_outer);
    int exhaustive = has_default;
    if (!exhaustive && enum_cls) exhaustive = enum_covered(enum_cls, &enum_seen);
    if (!exhaustive && sel->kind == TY_CLASS && type_seen.count) exhaustive = type_covered(sel->cls, &type_seen);
    if (is_expr) {
        if (!exhaustive && sel->kind != TY_ERROR) cerr(n, "switch expression does not cover all possible input values; add the missing cases or a 'default'");
        if (!result) result = ty_error();
        return result;
    }
    if (!exhaustive && type_seen.count && sel->kind != TY_ERROR) cerr(n, "pattern switch does not cover all possible input values; add a 'default'");
    return ty_prim(TY_VOID);
}

/* ---------------------------------------------------------------- try / catch */

static JType *throwable_type(void) {
    JClass *c = jt_well_known("ash.lang.Throwable");
    jt_complete(c);
    return c->self_type;
}

static void check_try(Ctx *cx, Node *n) {
    ExFrame fr = {0};
    fr.kind = F_TRY;
    fr.up = cx->ex;
    JList catches = {0}; /* JType* per catch type, in order */
    for (int i = 0; i < n->l1.count; i++) {
        Node *c = n->l1.items[i];
        for (int j = 0; j < c->l1.count; j++) {
            Node *tn = c->l1.items[j];
            JType *t = jt_resolve_type(tn, cx->cls, cx->method, 1);
            tn->type = t;
            if (t->kind == TY_ERROR) continue;
            if (t->kind != TY_CLASS || !ty_is_subclass(t->cls, throwable_type()->cls)) {
                cerr(tn, "%s is not an exception type; catch needs a subclass of Throwable", ty_str(t));
                continue;
            }
            for (int k = 0; k < catches.count; k++) {
                JType *prev = catches.items[k];
                if (ty_is_subclass(t->cls, prev->cls)) cerr(tn, "exception %s has already been caught by %s", ty_str(t), ty_str(prev));
            }
            j_list_push(&catches, t);
            j_list_push(&fr.types, t);
        }
    }
    cx->ex = &fr;
    check_stmt(cx, n->b);
    cx->ex = fr.up;
    for (int i = 0; i < n->l1.count; i++) {
        Node *c = n->l1.items[i];
        JType *vt = NULL;
        for (int j = 0; j < c->l1.count; j++) {
            JType *t = ((Node *)c->l1.items[j])->type;
            if (!t || t->kind != TY_CLASS) continue;
            if (ty_is_checked_exception(t) && strcmp(t->cls->qname, "ash.lang.Exception") != 0 && strcmp(t->cls->qname, "ash.lang.Throwable") != 0) {
                int thrown = 0;
                for (int k = 0; k < fr.thrown.count; k++) {
                    JType *th = fr.thrown.items[k];
                    if (ty_is_subclass(th->cls, t->cls) || ty_is_subclass(t->cls, th->cls)) thrown = 1;
                }
                if (!thrown) cerr(c->l1.items[j], "exception %s is never thrown in the body of the corresponding try statement", ty_str(t));
            }
            if (!vt) vt = t;
            else if (vt->cls != t->cls) vt = throwable_type();
        }
        int mark;
        push_scope(cx, &mark);
        if (vt) {
            JLocal *l = jt_declare_local(cx, c, c->name, vt, 0, 0);
            l->has_init = 1;
            c->sym = l;
        }
        check_block_body(cx, c->b);
        pop_scope(cx, mark);
    }
    if (n->c) check_stmt(cx, n->c);
}

/* ---------------------------------------------------------------- statements */

static void check_return(Ctx *cx, Node *n) {
    if (cx->lambda_depth == 0 && !cx->method) { cerr(n, "'return' is not allowed here"); return; }
    if (cx->ret == NULL) { /* inferring a lambda result */
        if (n->a) {
            JType *t = check_expr(cx, n->a, NULL);
            if (cx->ret_types && t->kind != TY_VOID) j_list_push(cx->ret_types, t);
        } else cx->ret_seen_void = 1;
        return;
    }
    if (cx->ret->kind == TY_VOID) {
        if (n->a) {
            JType *t = check_expr(cx, n->a, NULL);
            if (cx->in_ctor || cx->lambda_depth) cerr(n, "this %s cannot return a value", cx->lambda_depth ? "lambda" : "constructor");
            else if (t->kind != TY_ERROR) cerr(n, "void method '%s' cannot return a value", cx->method ? cx->method->name : "");
        }
        return;
    }
    if (!n->a) { cerr(n, "missing return value; the method must return %s", ty_str(cx->ret)); return; }
    JType *t = check_expr(cx, n->a, cx->ret);
    check_assignable(cx, n->a, t, cx->ret, "return value");
    if (cx->ret_types) j_list_push(cx->ret_types, t);
}

static int is_stmt_expr_node(Node *n) {
    return n->kind == N_ASSIGN || n->kind == N_CALL || n->kind == N_NEW || n->kind == N_POSTFIX ||
           (n->kind == N_UNARY && (!strcmp(n->name, "++") || !strcmp(n->name, "--")));
}

static JType *iter_elem(Ctx *cx, Node *it, JType *t) {
    (void)cx;
    if (t->kind == TY_ARRAY) return t->elem;
    JClass *ib = jt_find_class("ash.lang.Iterable");
    JType *s = ib && t->kind != TY_ERROR ? ty_as_super(t, ib) : NULL;
    if (s) return s->nargs ? s->args[0] : jt_object_type();
    if (t->kind != TY_ERROR) cerr(it, "for-each needs an array or an Iterable, found %s", ty_str(t));
    return ty_error();
}

void check_stmt(Ctx *cx, Node *n) {
    switch (n->kind) {
        case N_BLOCK: check_block_body(cx, n); break;
        case N_SEQ: for (int i = 0; i < n->l1.count; i++) check_stmt(cx, n->l1.items[i]); break;
        case N_EMPTY: break;
        case N_LOCAL: check_local(cx, n); break;
        case N_EXPRSTMT:
            check_expr(cx, n->a, NULL);
            if (!is_stmt_expr_node(n->a) && n->a->type->kind != TY_ERROR) cerr(n->a, "not a statement");
            break;
        case N_IF: {
            check_cond(cx, n->a);
            JList undo = {0};
            narrow_apply(cx, n->a, 1, &undo);
            int mark;
            push_scope(cx, &mark);
            check_stmt(cx, n->b);
            pop_scope(cx, mark);
            narrow_undo(&undo);
            if (n->c) {
                narrow_apply(cx, n->a, 0, &undo);
                push_scope(cx, &mark);
                check_stmt(cx, n->c);
                pop_scope(cx, mark);
                narrow_undo(&undo);
            }
            /* guard clause: `if (x == null) return;` leaves x non-null afterwards */
            if (!can_complete(n->b) && !n->c) narrow_apply(cx, n->a, 0, NULL);
            else if (n->c && !can_complete(n->c) && can_complete(n->b)) narrow_apply(cx, n->a, 1, NULL);
            break;
        }
        case N_WHILE: {
            check_cond(cx, n->a);
            JList undo = {0};
            narrow_apply(cx, n->a, 1, &undo);
            int mark;
            push_scope(cx, &mark);
            cx->loop_depth++;
            check_stmt(cx, n->b);
            cx->loop_depth--;
            pop_scope(cx, mark);
            narrow_undo(&undo);
            break;
        }
        case N_DO: {
            int mark;
            push_scope(cx, &mark);
            cx->loop_depth++;
            check_stmt(cx, n->b);
            cx->loop_depth--;
            pop_scope(cx, mark);
            check_cond(cx, n->a);
            break;
        }
        case N_FOR: {
            int mark;
            push_scope(cx, &mark);
            for (int i = 0; i < n->l1.count; i++) check_stmt(cx, n->l1.items[i]);
            if (n->a) check_cond(cx, n->a);
            for (int i = 0; i < n->l2.count; i++) {
                Node *u = n->l2.items[i];
                check_expr(cx, u, NULL);
                if (!is_stmt_expr_node(u) && u->type->kind != TY_ERROR) cerr(u, "not a statement");
            }
            cx->loop_depth++;
            check_stmt(cx, n->b);
            cx->loop_depth--;
            pop_scope(cx, mark);
            break;
        }
        case N_FOREACH: {
            JType *it = check_expr(cx, n->b, NULL);
            if (it->nullable) cerr(n->b, "possible null: iterating over nullable %s; check for null first", ty_str(it));
            JType *elem = iter_elem(cx, n->b, it);
            JType *vt = elem;
            if (n->a) {
                vt = jt_resolve_type(n->a, cx->cls, cx->method, 1);
                check_assignable(cx, n->b, elem, vt, "loop variable");
            }
            int mark;
            push_scope(cx, &mark);
            JLocal *l = jt_declare_local(cx, n, n->name, vt, 0, 0);
            l->has_init = 1;
            n->sym = l;
            n->type = vt;
            cx->loop_depth++;
            check_stmt(cx, n->c);
            cx->loop_depth--;
            pop_scope(cx, mark);
            break;
        }
        case N_RETURN: check_return(cx, n); break;
        case N_BREAK:
            if (!cx->loop_depth && !cx->switch_depth) cerr(n, "'break' outside of a loop or switch");
            break;
        case N_CONTINUE:
            if (!cx->loop_depth) cerr(n, "'continue' outside of a loop");
            break;
        case N_THROW: {
            JType *t = check_expr(cx, n->a, NULL);
            if (t->kind == TY_ERROR) break;
            if (t->kind != TY_CLASS || !ty_is_subclass(t->cls, throwable_type()->cls)) {
                cerr(n->a, "only Throwable can be thrown, found %s", ty_str(t));
                break;
            }
            if (t->nullable) cerr(n->a, "possible null: throwing nullable %s", ty_str(t));
            report_throw(cx, n, t);
            break;
        }
        case N_TRY: check_try(cx, n); break;
        case N_SWITCH: check_switch(cx, n, NULL); break;
        case N_CTORCALL: check_ctor_call(cx, n); break;
        default:
            cerr(n, "unexpected statement");
    }
}
