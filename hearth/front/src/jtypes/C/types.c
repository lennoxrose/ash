#include <stdlib.h>
#include <string.h>

#include "jtypes/H/jtypes.h"

static JType prims[TY_ERROR + 1];
static int prims_ready;

static void init_prims(void) {
    for (int i = 0; i <= TY_ERROR; i++) prims[i].kind = (TyKind)i;
    prims_ready = 1;
}

JType *ty_prim(TyKind k) {
    if (!prims_ready) init_prims();
    return &prims[k];
}

JType *ty_null(void) { return ty_prim(TY_NULL); }
JType *ty_error(void) { return ty_prim(TY_ERROR); }

JType *ty_class(JClass *c, JType **args, int nargs) {
    JType *t = j_alloc(sizeof(JType));
    t->kind = TY_CLASS;
    t->cls = c;
    t->args = args;
    t->nargs = nargs;
    return t;
}

JType *ty_array(JType *elem) {
    JType *t = j_alloc(sizeof(JType));
    t->kind = TY_ARRAY;
    t->elem = elem;
    return t;
}

JType *ty_tvar(JTVar *tv) {
    JType *t = j_alloc(sizeof(JType));
    t->kind = TY_TVAR;
    t->tv = tv;
    return t;
}

JType *ty_with_null(JType *t, int nullable) {
    if (t->kind != TY_CLASS && t->kind != TY_ARRAY && t->kind != TY_TVAR) return t;
    if (t->nullable == nullable) return t;
    JType *c = j_alloc(sizeof(JType));
    *c = *t;
    c->nullable = nullable;
    return c;
}

int ty_is_prim(JType *t) { return t->kind >= TY_BOOL && t->kind <= TY_DOUBLE; }
int ty_is_numeric(JType *t) { return t->kind >= TY_BYTE && t->kind <= TY_DOUBLE; }
int ty_is_integral(JType *t) { return t->kind >= TY_BYTE && t->kind <= TY_LONG; }
int ty_is_ref(JType *t) {
    return t->kind == TY_CLASS || t->kind == TY_ARRAY || t->kind == TY_TVAR || t->kind == TY_NULL;
}

int ty_is_string(JType *t) { return t->kind == TY_CLASS && t->cls == jt_string(); }
int ty_is_object(JType *t) { return t->kind == TY_CLASS && t->cls == jt_object(); }

int ty_same(JType *a, JType *b) {
    if (a == b) return 1;
    if (a->kind != b->kind) return 0;
    switch (a->kind) {
        case TY_CLASS:
            if (a->cls != b->cls) return 0;
            if (a->nargs && b->nargs) {
                if (a->nargs != b->nargs) return 0;
                for (int i = 0; i < a->nargs; i++) {
                    if (!ty_same(a->args[i], b->args[i])) return 0;
                }
            }
            return 1;
        case TY_ARRAY: return ty_same(a->elem, b->elem);
        case TY_TVAR: return a->tv == b->tv;
        default: return 1;
    }
}

const char *ty_str(JType *t) {
    JBuf b = {0};
    switch (t->kind) {
        case TY_VOID: return "void";
        case TY_BOOL: return "bool";
        case TY_BYTE: return "byte";
        case TY_SHORT: return "short";
        case TY_CHAR: return "char";
        case TY_INT: return "int";
        case TY_LONG: return "long";
        case TY_FLOAT: return "float";
        case TY_DOUBLE: return "double";
        case TY_NULL: return "null";
        case TY_ERROR: return "<error>";
        case TY_TVAR: j_buf_add(&b, t->tv->name); break;
        case TY_ARRAY: j_buf_add(&b, ty_str(t->elem)); j_buf_add(&b, "[]"); break;
        case TY_CLASS:
            j_buf_add(&b, t->cls->name);
            if (t->nargs) {
                j_buf_addc(&b, '<');
                for (int i = 0; i < t->nargs; i++) {
                    if (i) j_buf_add(&b, ", ");
                    j_buf_add(&b, ty_str(t->args[i]));
                }
                j_buf_addc(&b, '>');
            }
            break;
    }
    if (t->nullable) j_buf_addc(&b, '?');
    return b.data;
}

static const char *box_names[] = {
    [TY_BOOL] = "ash.lang.Boolean", [TY_BYTE] = "ash.lang.Byte", [TY_SHORT] = "ash.lang.Short",
    [TY_CHAR] = "ash.lang.Character", [TY_INT] = "ash.lang.Integer", [TY_LONG] = "ash.lang.Long",
    [TY_FLOAT] = "ash.lang.Float", [TY_DOUBLE] = "ash.lang.Double",
};

JType *ty_box(JType *t) {
    if (!ty_is_prim(t)) return t;
    JClass *c = jt_well_known(box_names[t->kind]);
    jt_complete(c);
    return c->self_type;
}

JType *ty_unbox(JType *t) {
    if (t->kind != TY_CLASS || !t->cls->qname) return t;
    for (int k = TY_BOOL; k <= TY_DOUBLE; k++) {
        if (strcmp(t->cls->qname, box_names[k]) == 0) return ty_prim((TyKind)k);
    }
    return t;
}

JType *ty_subst(JType *t, JList *tvars, JType **args, int nargs) {
    switch (t->kind) {
        case TY_TVAR:
            for (int i = 0; i < tvars->count && i < nargs; i++) {
                if (tvars->items[i] == t->tv) return ty_with_null(args[i], args[i]->nullable || t->nullable);
            }
            return t;
        case TY_ARRAY: {
            JType *e = ty_subst(t->elem, tvars, args, nargs);
            if (e == t->elem) return t;
            return ty_with_null(ty_array(e), t->nullable);
        }
        case TY_CLASS: {
            if (!t->nargs) return t;
            JType **na = j_alloc(sizeof(JType *) * (size_t)t->nargs);
            int changed = 0;
            for (int i = 0; i < t->nargs; i++) {
                na[i] = ty_subst(t->args[i], tvars, args, nargs);
                if (na[i] != t->args[i]) changed = 1;
            }
            if (!changed) return t;
            return ty_with_null(ty_class(t->cls, na, t->nargs), t->nullable);
        }
        default:
            return t;
    }
}

JType *ty_erase_bound(JType *t) {
    if (t->kind == TY_TVAR) return t->tv->bound ? ty_erase_bound(t->tv->bound) : jt_object_type();
    return t;
}

int ty_is_subclass(JClass *c, JClass *target) {
    if (c == target) return 1;
    if (target == jt_object()) return 1;
    jt_complete(c);
    if (c->super && ty_is_subclass(c->super->cls, target)) return 1;
    for (int i = 0; i < c->ifaces.count; i++) {
        if (ty_is_subclass(((JType *)c->ifaces.items[i])->cls, target)) return 1;
    }
    return 0;
}

JType *ty_as_super(JType *t, JClass *target) {
    if (t->kind == TY_TVAR) return ty_as_super(ty_erase_bound(t), target);
    if (t->kind == TY_ARRAY) return target == jt_object() ? jt_object_type() : NULL;
    if (t->kind != TY_CLASS) return NULL;
    if (t->cls == target) return t;
    JClass *c = t->cls;
    jt_complete(c);
    if (c->super) {
        JType *s = ty_subst(c->super, &c->tparams, t->args, t->nargs);
        JType *r = ty_as_super(s, target);
        if (r) return r;
    }
    for (int i = 0; i < c->ifaces.count; i++) {
        JType *s = ty_subst((JType *)c->ifaces.items[i], &c->tparams, t->args, t->nargs);
        JType *r = ty_as_super(s, target);
        if (r) return r;
    }
    if (target == jt_object()) return jt_object_type();
    return NULL;
}

static int prim_widens(TyKind from, TyKind to) {
    if (from == to) return 1;
    switch (from) {
        case TY_BYTE: return to == TY_SHORT || to == TY_INT || to == TY_LONG || to == TY_FLOAT || to == TY_DOUBLE;
        case TY_SHORT: return to == TY_INT || to == TY_LONG || to == TY_FLOAT || to == TY_DOUBLE;
        case TY_CHAR: return to == TY_INT || to == TY_LONG || to == TY_FLOAT || to == TY_DOUBLE;
        case TY_INT: return to == TY_LONG || to == TY_FLOAT || to == TY_DOUBLE;
        case TY_LONG: return to == TY_FLOAT || to == TY_DOUBLE;
        case TY_FLOAT: return to == TY_DOUBLE;
        default: return 0;
    }
}

static int args_compatible(JType *from, JType *to) {
    if (!from->nargs || !to->nargs || from->nargs != to->nargs) return 1;
    for (int i = 0; i < from->nargs; i++) {
        JType *a = from->args[i], *b = to->args[i];
        if (a->kind == TY_TVAR || b->kind == TY_TVAR) continue;
        if (ty_is_object(b)) continue;
        if (!ty_assignable(a, b, 1)) return 0;
    }
    return 1;
}

int ty_assignable(JType *from, JType *to, int lenient_null) {
    if (from->kind == TY_ERROR || to->kind == TY_ERROR) return 1;
    if (from->kind == TY_VOID || to->kind == TY_VOID) return 0;
    if (from->kind == TY_NULL) return ty_is_ref(to) && (to->nullable || lenient_null);
    if (ty_is_prim(from) && ty_is_prim(to)) return prim_widens(from->kind, to->kind) || (from->kind == TY_BOOL && to->kind == TY_BOOL);
    if (ty_is_prim(from)) { /* boxing */
        if (!ty_is_ref(to)) return 0;
        return ty_assignable(ty_box(from), ty_with_null(to, 1), 1);
    }
    if (ty_is_prim(to)) { /* unboxing */
        JType *u = ty_unbox(from);
        if (!ty_is_prim(u)) return 0;
        if (from->nullable && !lenient_null) return 0;
        return prim_widens(u->kind, to->kind);
    }
    if (from->nullable && !to->nullable && !lenient_null) return 0;
    if (to->kind == TY_TVAR) return from->kind == TY_TVAR && from->tv == to->tv;
    if (to->kind == TY_ARRAY) {
        if (from->kind != TY_ARRAY) return 0;
        if (ty_is_prim(from->elem) || ty_is_prim(to->elem)) return from->elem->kind == to->elem->kind;
        return ty_assignable(from->elem, to->elem, 1);
    }
    if (to->kind == TY_CLASS) {
        if (to->cls == jt_object()) return 1;
        if (from->kind == TY_ARRAY) return 0;
        JType *s = ty_as_super(from, to->cls);
        if (!s) return 0;
        return args_compatible(s, to);
    }
    return 0;
}

JType *ty_binary_numeric(JType *a, JType *b) {
    a = ty_unbox(a);
    b = ty_unbox(b);
    if (!ty_is_numeric(a) || !ty_is_numeric(b)) return NULL;
    if (a->kind == TY_DOUBLE || b->kind == TY_DOUBLE) return ty_prim(TY_DOUBLE);
    if (a->kind == TY_FLOAT || b->kind == TY_FLOAT) return ty_prim(TY_FLOAT);
    if (a->kind == TY_LONG || b->kind == TY_LONG) return ty_prim(TY_LONG);
    return ty_prim(TY_INT);
}

int ty_is_checked_exception(JType *t) {
    if (t->kind != TY_CLASS) return 0;
    JClass *ex = jt_well_known("ash.lang.Exception");
    JClass *rt = jt_well_known("ash.lang.RuntimeException");
    return ty_is_subclass(t->cls, ex) && !ty_is_subclass(t->cls, rt);
}
