#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jcommon/H/jcommon.h"
#include "jown/H/range.h"
#include "jlower/H/jlower.h"
#include "jtypes/H/check.h"

#define NEG_INF INT64_MIN
#define POS_INF INT64_MAX

typedef struct { int64_t lo, hi; } Iv;

/* a + b that stops at the ends of int64 instead of wrapping (the ends stand for infinity anyway) */
static int64_t sat_add(int64_t a, int64_t b) {
    int64_t r;
    if (!__builtin_add_overflow(a, b, &r)) return r;
    return b > 0 ? POS_INF : NEG_INF;
}

static uint64_t id_of(JLocal *l) { return (uint64_t)(uintptr_t)l; }

/* ---------------------------------------------------------------- types */

static int int_kind(JType *t) {
    return t && !t->nullable && (t->kind == TY_BYTE || t->kind == TY_SHORT || t->kind == TY_CHAR || t->kind == TY_INT || t->kind == TY_LONG);
}

static Iv type_range(TyKind k) {
    switch (k) {
        case TY_BYTE:  return (Iv){-128, 127};
        case TY_SHORT: return (Iv){-32768, 32767};
        case TY_CHAR:  return (Iv){0, 65535};
        case TY_INT:   return (Iv){INT32_MIN, INT32_MAX};
        default:       return (Iv){NEG_INF, POS_INF};
    }
}

static int tracked(JLocal *l) { return l && int_kind(l->type); }
static int tracked_array(JLocal *l) { return l && l->type && !l->type->nullable && l->type->kind == TY_ARRAY; }
static int tracked_string(JLocal *l) { return l && l->type && !l->type->nullable && ty_is_string(l->type); }

/* `s.length()` / `s.charAt(i)` on a String: no side effects, so they can be evaluated against one snapshot */
static int string_call(Node *e, const char *name) {
    JMethod *m = e->kind == N_CALL ? e->sym : NULL;
    return m && e->a && m->cls == jt_string() && !strcmp(m->name, name) && e->flags != 1;
}

/* ---------------------------------------------------------------- List sizes */

static int list_qname(JClass *c) {
    return c && c->qname && (!strcmp(c->qname, "ash.util.List") || !strcmp(c->qname, "ash.util.ArrayList"));
}

static int fresh_arraylist(Node *e) {
    JMethod *m = e && e->kind == N_NEW ? e->sym : NULL;
    return m && !e->b && e->l1.count == 0 && m->cls && m->cls->qname && !strcmp(m->cls->qname, "ash.util.ArrayList");
}

static int tracked_list(JLocal *l) { return l && l->rl_list == 1; }

/* `l.name(..)` on a tracked list local: the method name, else NULL */
static const char *list_call(Node *e) {
    JMethod *m = e->kind == N_CALL ? e->sym : NULL;
    if (!m || !e->a || e->flags == 1 || !list_qname(m->cls)) return NULL;
    if (e->a->kind != N_NAME || e->a->rk != R_LOCAL || !tracked_list(e->a->sym)) return NULL;
    return m->name;
}

/* the methods a tracked list may be used with, and their number of arguments */
static int list_method_ok(const char *name, int nargs) {
    return (!strcmp(name, "size") && nargs == 0) || (!strcmp(name, "isEmpty") && nargs == 0) ||
           (!strcmp(name, "get") && nargs == 1) || (!strcmp(name, "set") && nargs == 2) ||
           (!strcmp(name, "contains") && nargs == 1) || (!strcmp(name, "add") && (nargs == 1 || nargs == 2)) ||
           (!strcmp(name, "remove") && nargs == 1) || (!strcmp(name, "removeLast") && nargs == 0);
}

static int list_mutator(const char *name) {
    return !strcmp(name, "add") || !strcmp(name, "remove") || !strcmp(name, "removeLast");
}

/* ---- prescan: which list locals can be followed ---- */

static int ls_member_of(Node *p, Node *n) {
    return p && p->kind == N_CALL && p->a == n && p->sym && list_qname(((JMethod *)p->sym)->cls) && p->flags != 1 &&
           list_method_ok(((JMethod *)p->sym)->name, p->l1.count);
}

static void ls_candidates(Node *n) {
    if (!n) return;
    if (n->kind == N_LOCAL && n->sym) {
        JLocal *l = n->sym;
        l->rl_list = l->type && !l->type->nullable && l->type->kind == TY_CLASS && list_qname(l->type->cls) && fresh_arraylist(n->b) ? 1 : 0;
    }
    ls_candidates(n->a); ls_candidates(n->b); ls_candidates(n->c);
    JList *ls[] = {&n->l1, &n->l2, &n->l3, &n->l4, &n->l5};
    for (int k = 0; k < 5; k++) for (int i = 0; i < ls[k]->count; i++) ls_candidates(ls[k]->items[i]);
}

static void ls_uses(Node *n, Node *parent, int later) {
    if (!n) return;
    if (n->kind == N_LAMBDA || n->kind == N_METHODREF || n->kind == N_CLASS || n->kind == N_INTERFACE || n->kind == N_ENUM ||
        n->kind == N_RECORD || n->kind == N_METHOD || n->kind == N_CTOR || (n->kind == N_NEW && n->b)) later = 1;
    if (n->kind == N_ASSIGN && n->a && n->a->kind == N_NAME && n->a->rk == R_LOCAL && n->a->sym && tracked_list(n->a->sym)) {
        if (later || strcmp(n->name, "=") || !fresh_arraylist(n->b)) ((JLocal *)n->a->sym)->rl_list = 0;
        ls_uses(n->b, n, later);
        return;
    }
    if (n->kind == N_NAME && n->rk == R_LOCAL && n->sym && tracked_list(n->sym) && (later || !ls_member_of(parent, n))) ((JLocal *)n->sym)->rl_list = 0;
    ls_uses(n->a, n, later); ls_uses(n->b, n, later); ls_uses(n->c, n, later);
    JList *ls[] = {&n->l1, &n->l2, &n->l3, &n->l4, &n->l5};
    for (int k = 0; k < 5; k++) for (int i = 0; i < ls[k]->count; i++) ls_uses(ls[k]->items[i], n, later);
}

void rg_prescan(Node *body) {
    ls_candidates(body);
    ls_uses(body, NULL, 0);
}

/* ---------------------------------------------------------------- evaluation */

/* An expression that writes a variable or calls code cannot be evaluated against one snapshot. */
static int pure(Node *e) {
    if (!e) return 1;
    switch (e->kind) {
        case N_CALL: if (string_call(e, "length") && e->l1.count == 0) break;
                     if (string_call(e, "charAt") && e->l1.count == 1) break;
                     { const char *lc = list_call(e); if (lc && !strcmp(lc, "size") && e->l1.count == 0) break; }
                     return 0;
        case N_ASSIGN: case N_NEW: case N_LAMBDA: case N_POSTFIX: case N_SWITCH: return 0;
        case N_UNARY: if (e->name[0] == '+' && e->name[1] == '+') return 0;
                      if (e->name[0] == '-' && e->name[1] == '-') return 0;
                      break;
        default: break;
    }
    if (!pure(e->a) || !pure(e->b) || !pure(e->c)) return 0;
    for (int i = 0; i < e->l1.count; i++) if (!pure(e->l1.items[i])) return 0;
    return 1;
}

/* a constant the type checker already folds (and reports): literals, final constants and operators on them */
static int foldable(Node *e) {
    switch (e->kind) {
        case N_LIT_INT: case N_LIT_LONG: case N_LIT_CHAR: return 1;
        case N_NAME:
            if (e->rk == R_LOCAL && e->sym) return (((JLocal *)e->sym)->flags & M_FINAL) != 0;
            return e->rk == R_STATIC_FIELD && e->sym && (((JField *)e->sym)->flags & M_FINAL);
        case N_UNARY: return foldable(e->a);
        case N_BINARY: return foldable(e->a) && foldable(e->b);
        default: return 0;
    }
}

static Iv eval(RgCtx *r, Node *e);

static Iv top_of(Node *e) {
    return e->type && int_kind(e->type) ? type_range(e->type->kind) : (Iv){NEG_INF, POS_INF};
}

static int is_literal_zero(Node *e) {
    return e->kind == N_LIT_INT && strtoull(e->name, NULL, 0) == 0;
}

static Iv fit(Iv v, TyKind k) {                      /* a value of a narrower type than the arithmetic used */
    Iv t = type_range(k);
    return v.lo >= t.lo && v.hi <= t.hi ? v : t;
}

/* ---------------------------------------------------------------- lengths of Strings and array fields */

static int ascii_only(const char *s) {
    for (; *s; s++) if ((unsigned char)*s >= 0x80) return 0;
    return 1;
}

/* length interval of a String expression (literals, concatenations of known Strings, String locals) */
static int str_len(RgCtx *r, Node *e, Iv *out) {
    if (e->kind == N_LIT_STRING) {
        if (!ascii_only(e->name)) return 0;            /* bytes and characters could differ */
        out->lo = out->hi = (int64_t)strlen(e->name);
        return 1;
    }
    if (e->kind == N_NAME && e->rk == R_LOCAL && tracked_string(e->sym)) {
        int64_t lo, hi;
        if (!mnemo_len_get(r->m, id_of(e->sym), &lo, &hi)) return 0;
        *out = (Iv){lo, hi};
        return 1;
    }
    if (e->kind == N_BINARY && !strcmp(e->name, "+") && e->type && ty_is_string(e->type)) {
        Iv a, b;
        if (!e->a->type || !ty_is_string(e->a->type) || !e->b->type || !ty_is_string(e->b->type)) return 0;
        if (!str_len(r, e->a, &a) || !str_len(r, e->b, &b)) return 0;
        if (a.hi > INT32_MAX || b.hi > INT32_MAX) return 0;
        *out = (Iv){sat_add(a.lo, b.lo), sat_add(a.hi, b.hi)};
        return 1;
    }
    return 0;
}

/* an int constant the type checker could fold: literals, - + * and final static fields with such an initializer */
static int const_int(Node *e, int64_t *v, int depth) {
    if (!e || depth > 4 || !e->type || e->type->kind != TY_INT) return 0;
    switch (e->kind) {
        case N_LIT_INT: {
            if (strchr(e->name, '_')) return 0;
            unsigned long long x = strtoull(e->name, NULL, 0);
            if (x > 2147483647ULL) return 0;
            *v = (int64_t)x;
            return 1;
        }
        case N_UNARY: {
            int64_t a;
            if (strcmp(e->name, "-") || !const_int(e->a, &a, depth + 1)) return 0;
            *v = -a;
            return 1;
        }
        case N_BINARY: {
            int64_t a, b, o;
            if (e->name[1] != 0 || !strchr("+-*", e->name[0])) return 0;
            if (!const_int(e->a, &a, depth + 1) || !const_int(e->b, &b, depth + 1)) return 0;
            o = e->name[0] == '+' ? a + b : e->name[0] == '-' ? a - b : a * b;
            if (o < INT32_MIN || o > INT32_MAX) return 0;
            *v = o;
            return 1;
        }
        case N_NAME: case N_FIELDACC:
            if (e->sym && (e->rk == R_STATIC_FIELD)) {
                JField *f = e->sym;
                if ((f->flags & M_FINAL) && (f->flags & M_STATIC) && f->decl && f->decl->b) return const_int(f->decl->b, v, depth + 1);
            }
            return 0;
        default: return 0;
    }
}

/* the length of a freshly made array whose size is known when compiling */
static int new_array_len(Node *init, Iv *out) {
    if (!init) return 0;
    if (init->kind == N_ARRAYINIT) { *out = (Iv){init->l1.count, init->l1.count}; return 1; }
    if (init->kind == N_NEWARRAY) {
        if (init->b && init->b->kind == N_ARRAYINIT) { *out = (Iv){init->b->l1.count, init->b->l1.count}; return 1; }
        int64_t v;
        if (!init->b && init->l1.count && const_int(init->l1.items[0], &v, 0) && v >= 0) { *out = (Iv){v, v}; return 1; }
    }
    return 0;
}

static int field_node(Node *a) {
    return (a->kind == N_NAME || a->kind == N_FIELDACC) && a->sym && (a->rk == R_FIELD || a->rk == R_STATIC_FIELD || a->rk == R_THIS_FIELD);
}

static void field_assigned(JField *f, Node *rhs) {
    Iv len;
    if (f->rl_state != 1) return;
    if (!new_array_len(rhs, &len)) { f->rl_state = 0; return; }
    if (len.lo < f->rl_lo) f->rl_lo = len.lo;
    if (len.hi > f->rl_hi) f->rl_hi = len.hi;
}

static void scan_assigns(Node *n) {
    if (!n) return;
    if (n->kind == N_ASSIGN && field_node(n->a)) {
        JField *f = n->a->sym;
        if (strcmp(n->name, "=")) f->rl_state = 0; else field_assigned(f, n->b);
    }
    scan_assigns(n->a); scan_assigns(n->b); scan_assigns(n->c);
    JList *ls[] = {&n->l1, &n->l2, &n->l3, &n->l4, &n->l5};
    for (int k = 0; k < 5; k++) for (int i = 0; i < ls[k]->count; i++) scan_assigns(ls[k]->items[i]);
}

/* An array field of a plain class whose every assignment in the project makes an array of a size known when
 * compiling has that size (the hull of them) for good: an array cannot change length, and only the project's
 * own code can write the field. A field nobody assigns stays unknown (records and enums set theirs implicitly). */
void rg_prepare_project(JClass **all, int n) {
    for (int i = 0; i < n; i++) {
        JClass *c = all[i];
        if (!c->mem_on || c->kind != N_CLASS) continue;
        for (int k = 0; k < c->fields.count; k++) {
            JField *f = c->fields.items[k];
            f->rl_state = f->type && f->type->kind == TY_ARRAY && !f->is_enum_const ? 1 : 0;
            f->rl_lo = INT64_MAX;
            f->rl_hi = -1;
        }
    }
    for (int i = 0; i < n; i++) {
        JClass *c = all[i];
        if (!c->mem_on || c->kind != N_CLASS) continue;
        for (int k = 0; k < c->fields.count; k++) {
            JField *f = c->fields.items[k];
            if (f->rl_state == 1 && f->decl && f->decl->b) field_assigned(f, f->decl->b);
        }
        scan_assigns(c->decl);
    }
    for (int i = 0; i < n; i++) {
        JClass *c = all[i];
        if (!c->mem_on || c->kind != N_CLASS) continue;
        for (int k = 0; k < c->fields.count; k++) {
            JField *f = c->fields.items[k];
            if (f->rl_state == 1) f->rl_state = f->rl_hi >= 0 ? 2 : 0;
        }
    }
}

/* length interval of an array expression; returns 0 when nothing is known */
static int array_len(RgCtx *r, Node *a, Iv *out) {
    if (a->kind == N_NAME && a->rk == R_LOCAL && tracked_array(a->sym)) {
        int64_t lo, hi;
        if (!mnemo_len_get(r->m, id_of(a->sym), &lo, &hi)) return 0;
        *out = (Iv){lo, hi};
        return 1;
    }
    if (field_node(a)) {
        JField *f = a->sym;
        if (f->rl_state != 2) return 0;
        *out = (Iv){f->rl_lo, f->rl_hi};
        return 1;
    }
    if (a->kind == N_COND) {                          /* `c ? x : y`: the hull of the two arms (an array never changes length) */
        Iv x, y;
        if (!array_len(r, a->b, &x) || !array_len(r, a->c, &y)) return 0;
        *out = (Iv){x.lo < y.lo ? x.lo : y.lo, x.hi > y.hi ? x.hi : y.hi};
        return 1;
    }
    if (a->kind == N_ARRAYINIT) { *out = (Iv){a->l1.count, a->l1.count}; return 1; }
    if (a->kind == N_NEWARRAY) {
        if (a->b && a->b->kind == N_ARRAYINIT) { *out = (Iv){a->b->l1.count, a->b->l1.count}; return 1; }
        if (a->l1.count && pure(a->l1.items[0])) {
            Iv d = eval(r, a->l1.items[0]);
            if (d.hi < 0) return 0;
            *out = (Iv){d.lo < 0 ? 0 : d.lo, d.hi};
            return 1;
        }
    }
    return 0;
}

static Iv eval(RgCtx *r, Node *e) {
    Iv top = top_of(e);
    if (!int_kind(e->type) || !pure(e)) return top;
    switch (e->kind) {
        case N_LIT_INT: {
            unsigned long long v = strtoull(e->name, NULL, 0);
            if (strchr(e->name, '_')) return top;
            if (v > 2147483647ULL && v <= 0xFFFFFFFFULL && e->name[0] == '0') return (Iv){(int64_t)v - 4294967296LL, (int64_t)v - 4294967296LL};
            if (v > 2147483648ULL) return top;
            return (Iv){(int64_t)v, (int64_t)v};
        }
        case N_LIT_LONG: {
            if (strchr(e->name, '_')) return top;
            unsigned long long v = strtoull(e->name, NULL, 0);
            if (v > (unsigned long long)INT64_MAX) return top;
            return (Iv){(int64_t)v, (int64_t)v};
        }
        case N_NAME:
            if (e->rk == R_LOCAL && tracked(e->sym)) {
                int64_t lo, hi;
                if (mnemo_rng_get(r->m, id_of(e->sym), &lo, &hi)) return (Iv){lo, hi};
            }
            {
                int64_t v;
                if (e->rk == R_STATIC_FIELD && const_int(e, &v, 0)) return (Iv){v, v};
            }
            return top;
        case N_CALL: {
            Iv len;
            if (string_call(e, "length") && str_len(r, e->a, &len)) return len;
            if (list_call(e)) {                           /* size(): the only list call `pure` lets through */
                int64_t lo, hi;
                if (mnemo_len_get(r->m, id_of(e->a->sym), &lo, &hi)) return (Iv){lo, hi};
                return (Iv){0, INT32_MAX};
            }
            return string_call(e, "length") ? (Iv){0, INT32_MAX} : top;
        }
        case N_FIELDACC:
            if (e->rk == R_ARRAYLEN) {
                Iv len;
                if (array_len(r, e->a, &len)) return len;
                return (Iv){0, INT32_MAX};
            }
            {
                int64_t v;
                if (e->rk == R_STATIC_FIELD && const_int(e, &v, 0)) return (Iv){v, v};
            }
            return top;
        case N_UNARY:
            if (!strcmp(e->name, "-")) {
                Iv a = eval(r, e->a);
                if (a.lo == NEG_INF) return top;
                Iv n = {a.hi == POS_INF ? NEG_INF : -a.hi, -a.lo};
                return e->type->kind == TY_INT ? fit(n, TY_INT) : n;
            }
            if (!strcmp(e->name, "+")) return eval(r, e->a);
            return top;
        case N_CAST: {
            if (!int_kind(e->b->type)) return top;
            Iv a = eval(r, e->b);
            Iv t = type_range(e->type->kind);
            return a.lo >= t.lo && a.hi <= t.hi ? a : top;
        }
        case N_COND: {
            Iv b = eval(r, e->b), c = eval(r, e->c);
            return (Iv){b.lo < c.lo ? b.lo : c.lo, b.hi > c.hi ? b.hi : c.hi};
        }
        case N_BINARY: {
            char op = e->name[0];
            if (e->name[1] != 0 || !strchr("+-*/%&", op)) return top;
            Iv a = eval(r, e->a), b = eval(r, e->b), o;
            int rc = mnemo_iv_op((uint8_t)op, a.lo, a.hi, b.lo, b.hi, &o.lo, &o.hi);
            if (rc != 1) return top;
            if (e->type->kind == TY_LONG) return o;
            Iv t = type_range(e->type->kind);
            if (o.lo >= t.lo && o.hi <= t.hi) return o;
            if (!jl_wrap_int && o.lo <= t.hi && o.hi >= t.lo) {           /* it may overflow: past that point it fails */
                o.lo = o.lo < t.lo ? t.lo : o.lo;
                o.hi = o.hi > t.hi ? t.hi : o.hi;
                return o;
            }
            return t;
        }
        default: return top;
    }
}

/* ---------------------------------------------------------------- diagnostics */

static int already(RgCtx *r, Node *n) {
    for (int i = 0; i < r->reported.count; i++) if (r->reported.items[i] == n) return 1;
    j_list_push(&r->reported, n);
    return 0;
}

#define REPORT(r, n, code, ...) do { if (!(r)->mute && !already((r), (n))) { j_diag_code(code); cerr((n), __VA_ARGS__); } } while (0)
/* reported once per node `n`, with the caret on `at` (the token to change: the index, the divisor, the call) */
#define REPORT_AT(r, n, at, code, ...) do { if (!(r)->mute && !already((r), (n))) { j_diag_code(code); cerr((at), __VA_ARGS__); } } while (0)

static void show(char *buf, size_t sz, Iv v) {
    if (v.lo == v.hi) snprintf(buf, sz, "%lld", (long long)v.lo);
    else snprintf(buf, sz, "%lld to %lld", (long long)v.lo, (long long)v.hi);
}

void rg_check_index(RgCtx *r, Node *n) {
    if (!n->a->type || n->a->type->kind != TY_ARRAY || !pure(n->b) || !int_kind(n->b->type)) return;
    Iv i = eval(r, n->b);
    char b[64];
    if (i.hi < 0) {
        if (foldable(n->b)) return;                       /* check_const_index reports these */
        show(b, sizeof b, i);
        REPORT_AT(r, n, n->b, "E0201", "array index is always negative here (%s); array indexes start at 0, so fix the index or check it before this access", b);
        return;
    }
    Iv len;
    if (!array_len(r, n->a, &len)) return;
    if (i.lo >= len.hi) {
        int final_arr = (n->a->kind == N_NAME && n->a->rk == R_LOCAL && n->a->sym && (((JLocal *)n->a->sym)->flags & M_FINAL)) ||
                        n->a->kind == N_ARRAYINIT || n->a->kind == N_NEWARRAY;
        if (foldable(n->b) && final_arr) return;           /* check_const_index reports these */
        char l[64];
        show(b, sizeof b, i);
        show(l, sizeof l, len);
        REPORT_AT(r, n, n->b, "E0202", "array index is always out of bounds here: the index is %s and the array length is %s; valid indexes are 0 to length - 1, so fix the index or the array size, or check the index first", b, l);
    }
}

/* s.charAt(i) where i is always negative or always past the known length of s */
void rg_check_call(RgCtx *r, Node *n) {
    const char *lc = list_call(n);
    if (lc && (!strcmp(lc, "get") || !strcmp(lc, "set") || !strcmp(lc, "remove")) && n->l1.count >= 1) {
        Node *idx = n->l1.items[0];
        if (!pure(idx) || !int_kind(idx->type)) return;
        Iv i = eval(r, idx), len;
        int64_t lo, hi;
        char b[64], l[64];
        show(b, sizeof b, i);
        if (i.hi < 0) { REPORT_AT(r, n, idx, "E0203", "list index is always negative here (%s); list indexes start at 0, so fix the index or check it before this call", b); return; }
        if (!mnemo_len_get(r->m, id_of(n->a->sym), &lo, &hi)) return;
        len = (Iv){lo, hi};
        if (i.lo < len.hi) return;
        show(l, sizeof l, len);
        REPORT_AT(r, n, idx, "E0204", "list index is always out of bounds here: the index is %s and the list size is %s; valid indexes are 0 to size - 1, so fix the index, add the element first, or check size() before this call", b, l);
        return;
    }
    if (!string_call(n, "charAt") || n->l1.count != 1) return;
    Node *idx = n->l1.items[0];
    if (!pure(n->a) || !pure(idx) || !int_kind(idx->type)) return;
    Iv i = eval(r, idx), len;
    char b[64];
    if (i.hi < 0) {
        show(b, sizeof b, i);
        REPORT_AT(r, n, idx, "E0205", "string index is always negative here (%s); string indexes start at 0, so fix the index or check it before this call", b);
        return;
    }
    if (!str_len(r, n->a, &len) || i.lo < len.hi) return;
    char l[64];
    show(b, sizeof b, i);
    show(l, sizeof l, len);
    REPORT_AT(r, n, idx, "E0206", "string index is always out of bounds here: the index is %s and the string length is %s; valid indexes are 0 to length() - 1, so fix the index or check length() first", b, l);
}

/* `switch (x) { case 3, 4: ...`: inside that case x is one of the labels */
void rg_assume_case(RgCtx *r, Node *scrut, Node *kase) {
    if (kase->flags == 1 || !kase->l1.count) return;      /* default (also merged `case A: default:`) */
    if (scrut->kind != N_NAME || scrut->rk != R_LOCAL || !tracked(scrut->sym)) return;
    Iv hull = {POS_INF, NEG_INF};
    for (int i = 0; i < kase->l1.count; i++) {
        Node *lab = kase->l1.items[i];
        if (!int_kind(lab->type) || !pure(lab)) return;     /* patterns, null, Strings, enums */
        Iv v = eval(r, lab);
        if (v.lo == NEG_INF || v.hi == POS_INF) return;
        if (v.lo < hull.lo) hull.lo = v.lo;
        if (v.hi > hull.hi) hull.hi = v.hi;
    }
    Iv t = type_range(((JLocal *)scrut->sym)->type->kind);
    if (hull.lo < t.lo) hull.lo = t.lo;
    if (hull.hi > t.hi) hull.hi = t.hi;
    mnemo_rng_refine(r->m, id_of(scrut->sym), hull.lo, hull.hi);
}

static void overflow(RgCtx *r, Node *n, const char *what, Iv o) {
    char b[64];
    show(b, sizeof b, o);
    REPORT(r, n, "E0207", "arithmetic overflow: %s is always outside the range of int here (%s); use long, or --wrap-int for wrap-around", what, b);
}

void rg_check_binary(RgCtx *r, Node *n) {
    if (!int_kind(n->type) || n->name[1] != 0 || !strchr("+-*/%", n->name[0])) return;
    if (!pure(n->a) || !pure(n->b) || !int_kind(n->a->type) || !int_kind(n->b->type)) return;
    char op = n->name[0];
    Iv a = eval(r, n->a), b = eval(r, n->b), o;
    if (op == '/' || op == '%') {
        if (b.lo == 0 && b.hi == 0 && !is_literal_zero(n->b))
            REPORT_AT(r, n, n->b, "E0208", "division by zero: the divisor is always 0 here; check the divisor before dividing, or use a different one");
        return;
    }
    if (n->type->kind != TY_INT || jl_wrap_int || foldable(n)) return;
    if (mnemo_iv_op((uint8_t)op, a.lo, a.hi, b.lo, b.hi, &o.lo, &o.hi) == 1 && (o.lo > INT32_MAX || o.hi < INT32_MIN))
        overflow(r, n, "this expression", o);
}

void rg_check_newarray(RgCtx *r, Node *n) {
    if (!n->l1.count) return;
    Node *d = n->l1.items[0];
    if (!pure(d) || !int_kind(d->type) || foldable(d)) return;
    Iv v = eval(r, d);
    if (v.hi < 0) {
        char b[64];
        show(b, sizeof b, v);
        REPORT_AT(r, n, d, "E0209", "negative array size: the length is always negative here (%s); an array needs a length of 0 or more", b);
    }
}

/* ---------------------------------------------------------------- state updates */

static void set_len_from(RgCtx *r, JLocal *l, Node *init) {
    Iv len;
    if (init && array_len(r, init, &len)) mnemo_len_set(r->m, id_of(l), len.lo, len.hi);
    else mnemo_len_clear(r->m, id_of(l));
}

static void set_value(RgCtx *r, JLocal *l, Iv v) {
    Iv t = type_range(l->type->kind);
    if (v.lo < t.lo || v.hi > t.hi) {                   /* narrowing assignment or wrap-around: anything of the type */
        v = t;
    }
    if (v.lo == NEG_INF && v.hi == POS_INF) mnemo_rng_clear(r->m, id_of(l));
    else mnemo_rng_set(r->m, id_of(l), v.lo, v.hi);
}

/* after `l.add(..)` / `l.remove(i)` / `l.removeLast()` on a tracked list: the size changes by one */
void rg_call_effect(RgCtx *r, Node *n) {
    const char *lc = list_call(n);
    if (!lc || !list_mutator(lc)) return;
    uint64_t id = id_of(n->a->sym);
    int64_t lo, hi;
    if (!mnemo_len_get(r->m, id, &lo, &hi)) return;
    if (!strcmp(lc, "add")) {
        lo = sat_add(lo, 1);
        hi = hi >= INT32_MAX ? POS_INF : sat_add(hi, 1);
    } else {                                        /* the call had at least one element to take, or it failed */
        lo = lo > 1 ? lo - 1 : 0;
        hi = hi == POS_INF ? hi : (hi > 1 ? hi - 1 : 0);
    }
    mnemo_len_set(r->m, id, lo, hi);
}

void rg_forget(RgCtx *r, JLocal *l) {
    if (!l) return;
    mnemo_rng_clear(r->m, id_of(l));
    mnemo_len_clear(r->m, id_of(l));
}

static void set_str_len_from(RgCtx *r, JLocal *l, Node *init) {
    Iv len;
    if (init && str_len(r, init, &len)) mnemo_len_set(r->m, id_of(l), len.lo, len.hi);
    else mnemo_len_clear(r->m, id_of(l));
}

void rg_declare(RgCtx *r, Node *n) {
    JLocal *l = n->sym;
    if (!l) return;
    if (tracked_list(l) && n->b) mnemo_len_set(r->m, id_of(l), 0, 0);       /* rg_prescan: a fresh ArrayList */
    else if (tracked_string(l) && n->b) { rg_forget(r, l); set_str_len_from(r, l, n->b); }
    else if (tracked(l) && n->b) set_value(r, l, eval(r, n->b));
    else if (tracked_array(l) && n->b) set_len_from(r, l, n->b);
    else rg_forget(r, l);
}

void rg_assign(RgCtx *r, Node *n) {
    if (n->a->kind != N_NAME || n->a->rk != R_LOCAL || !n->a->sym) return;
    JLocal *l = n->a->sym;
    const char *op = n->name;
    if (tracked_list(l)) {
        if (!strcmp(op, "=")) mnemo_len_set(r->m, id_of(l), 0, 0); else rg_forget(r, l);
        return;
    }
    if (tracked_array(l)) {
        if (!strcmp(op, "=")) set_len_from(r, l, n->b); else rg_forget(r, l);
        return;
    }
    if (tracked_string(l)) {
        if (!strcmp(op, "=")) { set_str_len_from(r, l, n->b); return; }
        Iv a, b;
        if (!strcmp(op, "+=") && str_len(r, n->a, &a) && n->b->type && ty_is_string(n->b->type) && str_len(r, n->b, &b) && a.hi <= INT32_MAX && b.hi <= INT32_MAX)
            mnemo_len_set(r->m, id_of(l), sat_add(a.lo, b.lo), sat_add(a.hi, b.hi));
        else rg_forget(r, l);
        return;
    }
    if (!tracked(l)) return;
    if (!strcmp(op, "=")) { set_value(r, l, eval(r, n->b)); return; }
    char c = op[0];
    if (op[1] != '=' || op[2] != 0 || !strchr("+-*/%&", c) || !pure(n->b) || !int_kind(n->b->type)) { rg_forget(r, l); return; }
    int64_t lo, hi;
    Iv a = mnemo_rng_get(r->m, id_of(l), &lo, &hi) ? (Iv){lo, hi} : type_range(l->type->kind);
    Iv b = eval(r, n->b), o;
    int rc = mnemo_iv_op((uint8_t)c, a.lo, a.hi, b.lo, b.hi, &o.lo, &o.hi);
    if (rc == 3 && !is_literal_zero(n->b)) REPORT_AT(r, n, n->b, "E0208", "division by zero: the divisor is always 0 here; check the divisor before dividing, or use a different one");
    if (rc != 1) { rg_forget(r, l); return; }
    if (l->type->kind == TY_INT && !jl_wrap_int && (o.lo > INT32_MAX || o.hi < INT32_MIN) && (c == '+' || c == '-' || c == '*'))
        overflow(r, n, "this update", o);
    if (l->type->kind == TY_INT && !jl_wrap_int) {         /* continuing paths did not overflow */
        o.lo = o.lo < INT32_MIN ? INT32_MIN : o.lo;
        o.hi = o.hi > INT32_MAX ? INT32_MAX : o.hi;
        if (o.lo > o.hi) { rg_forget(r, l); return; }
    }
    set_value(r, l, o);
}

void rg_incdec(RgCtx *r, Node *n) {
    Node *t = n->a;
    if (!t || t->kind != N_NAME || t->rk != R_LOCAL || !tracked(t->sym)) return;
    JLocal *l = t->sym;
    int64_t lo, hi;
    Iv a = mnemo_rng_get(r->m, id_of(l), &lo, &hi) ? (Iv){lo, hi} : type_range(l->type->kind);
    int d = n->name[0] == '+' ? 1 : -1;
    Iv o = {a.lo == NEG_INF ? NEG_INF : sat_add(a.lo, d), a.hi == POS_INF ? POS_INF : sat_add(a.hi, d)};
    if (l->type->kind == TY_INT && !jl_wrap_int) {
        if (a.lo == a.hi && (o.lo > INT32_MAX || o.hi < INT32_MIN)) overflow(r, n, "this update", o);
        o.lo = o.lo < INT32_MIN ? INT32_MIN : o.lo;
        o.hi = o.hi > INT32_MAX ? INT32_MAX : o.hi;
        if (o.lo > o.hi) { rg_forget(r, l); return; }
    }
    set_value(r, l, o);
}

/* ---------------------------------------------------------------- conditions */

static int is_cmp(const char *s) {
    return !strcmp(s, "<") || !strcmp(s, "<=") || !strcmp(s, ">") || !strcmp(s, ">=") || !strcmp(s, "==") || !strcmp(s, "!=");
}

/* `x op v` holds where x is a tracked local and v is a value interval; negation flips the operator */
static void assume_cmp(RgCtx *r, JLocal *x, const char *op, Iv v, int truth) {
    char o[3];
    strcpy(o, op);
    if (!truth) {
        if (o[0] == '<') strcpy(o, o[1] ? ">" : ">=");
        else if (o[0] == '>') strcpy(o, o[1] ? "<" : "<=");
        else if (o[0] == '=') strcpy(o, "!=");
        else strcpy(o, "==");
    }
    int64_t lo = NEG_INF, hi = POS_INF;
    if (!strcmp(o, "<"))  { if (v.hi == NEG_INF) return; hi = v.hi == POS_INF ? POS_INF : sat_add(v.hi, -1); }
    else if (!strcmp(o, "<=")) hi = v.hi;
    else if (!strcmp(o, ">"))  { lo = v.lo == NEG_INF ? NEG_INF : sat_add(v.lo, 1); }
    else if (!strcmp(o, ">=")) lo = v.lo;
    else if (!strcmp(o, "==")) { lo = v.lo; hi = v.hi; }
    else {                                              /* != c: only an end of the known range can be cut off */
        int64_t a, b;
        if (v.lo != v.hi || !mnemo_rng_get(r->m, id_of(x), &a, &b)) return;
        if (a == v.lo && a != b) lo = sat_add(a, 1);
        else if (b == v.lo && a != b) hi = sat_add(b, -1);
        else if (a == b && a == v.lo) { mnemo_rng_refine(r->m, id_of(x), 1, 0); return; }   /* x is exactly c: x != c is impossible */
        else return;
    }
    Iv t = type_range(x->type->kind);
    if (lo < t.lo) lo = t.lo;
    if (hi > t.hi) hi = t.hi;
    mnemo_rng_refine(r->m, id_of(x), lo, hi);
}

static const char *flip(const char *op) {
    if (!strcmp(op, "<")) return ">";
    if (!strcmp(op, "<=")) return ">=";
    if (!strcmp(op, ">")) return "<";
    if (!strcmp(op, ">=")) return "<=";
    return op;
}

/* Does `e` write local `l` (assignment, ++ / --, a `&mut` borrow)? */
static int writes_local(Node *e, JLocal *l) {
    if (!e) return 0;
    Node *t = NULL;
    if (e->kind == N_ASSIGN) t = e->a;
    else if ((e->kind == N_UNARY || e->kind == N_POSTFIX) && e->name && ((e->name[0] == '+' && e->name[1] == '+') || (e->name[0] == '-' && e->name[1] == '-'))) t = e->a;
    else if (e->kind == N_UNARY && e->name && e->name[0] == '&' && e->name[1] == 'm') t = e->a;
    if (t && t->kind == N_NAME && t->rk == R_LOCAL && t->sym == l) return 1;
    if (writes_local(e->a, l) || writes_local(e->b, l) || writes_local(e->c, l)) return 1;
    JList *ls[] = {&e->l1, &e->l2, &e->l3, &e->l4, &e->l5};
    for (int k = 0; k < 5; k++) for (int i = 0; i < ls[k]->count; i++) if (writes_local(ls[k]->items[i], l)) return 1;
    return 0;
}

/* Does `top` write a local that `leaf` reads? Then the state the walk reached after `top` no longer describes the
 * moment `leaf` was evaluated (`x < 5 && x++ > 0`), and a fact taken from `leaf` would be stale. */
static int stale(Node *leaf, Node *top) {
    if (!leaf) return 0;
    if (leaf->kind == N_NAME && leaf->rk == R_LOCAL && leaf->sym && writes_local(top, leaf->sym)) return 1;
    if (stale(leaf->a, top) || stale(leaf->b, top) || stale(leaf->c, top)) return 1;
    for (int i = 0; i < leaf->l1.count; i++) if (stale(leaf->l1.items[i], top)) return 1;
    return 0;
}

static void assume(RgCtx *r, Node *c, int truth, Node *top) {
    switch (c->kind) {
        case N_LIT_BOOL:
            if ((strcmp(c->name, "true") == 0) != truth) mnemo_kill(r->m);
            return;
        case N_UNARY:
            if (!strcmp(c->name, "!")) assume(r, c->a, !truth, top);
            return;
        case N_BINARY: {
            const char *op = c->name;
            int and = !strcmp(op, "&&"), or = !strcmp(op, "||");
            if (and || or) {
                if (and == truth) {                      /* a && b true, or a || b false: both parts hold */
                    assume(r, c->a, truth, top);
                    assume(r, c->b, truth, top);
                } else {                                 /* one of the two parts decides */
                    uint32_t s0 = mnemo_save(r->m);
                    assume(r, c->a, truth, top);
                    uint32_t s1 = mnemo_save(r->m);
                    mnemo_restore(r->m, s0);
                    assume(r, c->a, !truth, top);
                    assume(r, c->b, truth, top);
                    mnemo_join(r->m, s1);
                    mnemo_release(r->m, s0);
                    mnemo_release(r->m, s1);
                }
                return;
            }
            if (!is_cmp(op) || !pure(c->a) || !pure(c->b) || stale(c, top)) return;
            if (c->a->kind == N_NAME && c->a->rk == R_LOCAL && tracked(c->a->sym) && int_kind(c->b->type))
                assume_cmp(r, c->a->sym, op, eval(r, c->b), truth);
            else if (c->b->kind == N_NAME && c->b->rk == R_LOCAL && tracked(c->b->sym) && int_kind(c->a->type))
                assume_cmp(r, c->b->sym, flip(op), eval(r, c->a), truth);
            return;
        }
        default: return;
    }
}

void rg_assume(RgCtx *r, Node *c, int truth) { assume(r, c, truth, c); }

/* ---------------------------------------------------------------- recursion without a way out */

static Node *calls_self(Node *e, JMethod *m) {
    if (!e) return NULL;
    if (e->kind == N_LAMBDA) return NULL;
    if ((e->kind == N_CALL) && e->sym == m && (e->a == NULL || e->a->kind == N_THIS)) return e;
    if (e->kind == N_COND) return calls_self(e->a, m);                       /* only the condition always runs */
    if (e->kind == N_BINARY && (!strcmp(e->name, "&&") || !strcmp(e->name, "||"))) return calls_self(e->a, m);
    if (e->kind == N_SWITCH) return calls_self(e->a, m);
    Node *c;
    if ((c = calls_self(e->a, m)) || (c = calls_self(e->b, m)) || (c = calls_self(e->c, m))) return c;
    for (int i = 0; i < e->l1.count; i++) if ((c = calls_self(e->l1.items[i], m))) return c;
    return NULL;
}

void rg_check_recursion(JMethod *m) {
    if (!m->decl || !m->decl->b || m->decl->b->kind != N_BLOCK) return;
    if (m->flags & (M_ABSTRACT | M_NATIVE)) return;
    JList *body = &m->decl->b->l1;
    for (int i = 0; i < body->count; i++) {
        Node *s = body->items[i];
        Node *e = NULL;
        if (s->kind == N_EXPRSTMT) e = s->a;
        else if (s->kind == N_RETURN) e = s->a;
        else if (s->kind == N_LOCAL) e = s->b;
        else return;                                         /* a branch, loop, try or throw: it may end */
        if (!e) { if (s->kind == N_RETURN) return; continue; }
        /* an instance method can be overridden: only a static, private or final one is certain to call itself */
        if (!(m->flags & (M_STATIC | M_PRIVATE | M_FINAL))) return;
        Node *call = calls_self(e, m);
        if (call) {
            j_diag_code("E0401"), cerr(call, "infinite recursion: '%s' calls itself on every path before any condition, so it can never return; add a base case (an `if` that returns before this call)", m->name);
            return;
        }
        if (s->kind == N_RETURN) return;
    }
}

/* ---------------------------------------------------------------- exceptions */

/* A catch block can be entered from the middle of the try block, so what the block assigns is unknown there. */
void rg_forget_assigned(RgCtx *r, Node *n) {
    if (!n) return;
    Node *t = NULL;
    if (n->kind == N_ASSIGN) t = n->a;
    else if ((n->kind == N_UNARY || n->kind == N_POSTFIX) && (!strcmp(n->name, "++") || !strcmp(n->name, "--"))) t = n->a;
    else if (n->kind == N_UNARY && n->name[0] == '&' && n->name[1] == 'm') t = n->a;
    if (t && t->kind == N_NAME && t->rk == R_LOCAL) rg_forget(r, t->sym);
    if (n->kind == N_CALL) { const char *lc = list_call(n); if (lc && list_mutator(lc)) rg_forget(r, n->a->sym); }
    rg_forget_assigned(r, n->a);
    rg_forget_assigned(r, n->b);
    rg_forget_assigned(r, n->c);
    JList *ls[] = {&n->l1, &n->l2, &n->l3, &n->l4, &n->l5};
    for (int k = 0; k < 5; k++) for (int i = 0; i < ls[k]->count; i++) rg_forget_assigned(r, ls[k]->items[i]);
}
