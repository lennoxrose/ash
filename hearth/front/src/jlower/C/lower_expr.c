#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jlower/H/jlower_internal.h"

/* Expression lowering: new-Ash typed AST -> old-Ash expression text. */

LState L;
int jl_wrap_int = 0;

void lerr(Node *at, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char buf[512];
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    cerr(at, "backend: %s", buf);
}

/* ---------------------------------------------------------------- naming */

static const char *reserved[] = {
    "local", "say", "given", "otherwise", "during", "each", "stop", "next", "forge", "yield", "attempt",
    "handle", "raise", "yes", "no", "none", "and", "or", "not", "in", "as", "self", "len", "type", "str",
    "num", "exit", "sqrt", "abs", "floor", "chr", "ord", "push", "pop", "insert", "slice", "delete", "sort",
    "keys", "values", "has", "map", "filter", "reduce", "split", "join", "substring", "indexOf", "contains",
    "starts_with", "ends_with", "replace", "repeat", "upper", "lower", "trim", "input", "read_file",
    "write_file", "append_file", "file_exists", "rename_file", "delete_file", "make_dir", "list_dir", NULL
};

char *sname(const char *n) {
    for (int i = 0; reserved[i]; i++) {
        if (strcmp(n, reserved[i]) == 0) return j_fmt("%s_", n);
    }
    return (char *)n;
}

char *quote_str(const char *s) {
    JBuf b = {0};
    j_buf_addc(&b, '"');
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        switch (*p) {
            case '"': j_buf_add(&b, "\\\""); break;
            case '\\': j_buf_add(&b, "\\\\"); break;
            case '\n': j_buf_add(&b, "\\n"); break;
            case '\t': j_buf_add(&b, "\\t"); break;
            case '\r': j_buf_add(&b, "\\r"); break;
            default: j_buf_addc(&b, (char)*p);
        }
    }
    j_buf_addc(&b, '"');
    return b.data;
}

char *new_temp(void) { return j_fmt("__t%d", L.tmp++); }

/* Classes whose generated code is referenced; only these are emitted (the old VM allows 256
 * module-level variables, and every class needs three). */
static JList used;
void mark_used(JClass *c) {
    for (int i = 0; i < used.count; i++) if (used.items[i] == c) return;
    j_list_push(&used, c);
}
int used_count(void) { return used.count; }
JClass *used_at(int i) { return used.items[i]; }

char *cls_state(JClass *c) { mark_used(c); return j_fmt("%s__S", c->mangled); }
char *cls_is(JClass *c) { mark_used(c); return j_fmt("%s__IS", c->mangled); }
char *cls_vt(JClass *c) { mark_used(c); return j_fmt("%s__VT", c->mangled); }

char *fn_name(JMethod *m, const char *kind) {
    mark_used(m->cls);
    if (kind) return j_fmt("%s__%s_%d", m->cls->mangled, kind, m->id);
    return j_fmt("%s__%s_%d", m->cls->mangled, m->name, m->id);
}

static JMethod *override_root(JMethod *m) {
    JMethod *root = m;
    for (int i = 0; i < 16; i++) {
        JMethod *o = jt_find_overridden(root->cls, root);
        if (!o) break;
        root = o;
    }
    return root;
}

char *method_key(JMethod *m) {
    JMethod *r = override_root(m);
    int dup = 0;
    jt_complete(r->cls);
    for (int i = 0; i < r->cls->methods.count; i++) {
        JMethod *o = r->cls->methods.items[i];
        if (o != r && !strcmp(o->name, r->name) && o->params.count == r->params.count) dup = 1;
    }
    JBuf b = {0};
    j_buf_addf(&b, "%s/%d", r->name, r->params.count);
    if (dup) {
        for (int i = 0; i < r->params.count; i++) {
            j_buf_addc(&b, '_');
            j_buf_add(&b, ty_str(ty_erase_bound(((JParam *)r->params.items[i])->type)));
        }
    }
    return b.data;
}

/* ---------------------------------------------------------------- aliases for bindings */

static JList alias_l, alias_t;
void alias_set(JLocal *l, const char *text) {
    j_list_push(&alias_l, l);
    j_list_push(&alias_t, (void *)text);
}
char *alias_get(JLocal *l) {
    for (int i = alias_l.count - 1; i >= 0; i--) if (alias_l.items[i] == l) return alias_t.items[i];
    return NULL;
}

/* ---------------------------------------------------------------- types */

int is_native_value_class(JClass *c) {
    static const char *nat[] = {
        "ash.lang.String", "ash.lang.Integer", "ash.lang.Long", "ash.lang.Short", "ash.lang.Byte", "ash.lang.Float",
        "ash.lang.Double", "ash.lang.Boolean", "ash.lang.Character", "ash.lang.Number", "ash.util.List",
        "ash.util.ArrayList", "ash.util.Map", "ash.util.HashMap", "ash.util.Set", "ash.util.HashSet", "ash.util.Stream", "ash.lang.Object", NULL
    };
    for (int i = 0; nat[i]; i++) if (!strcmp(c->qname, nat[i])) return 1;
    return 0;
}

int is_array_list(JType *t) {
    return t->kind == TY_CLASS && (!strcmp(t->cls->qname, "ash.util.List") || !strcmp(t->cls->qname, "ash.util.ArrayList") ||
                                   !strcmp(t->cls->qname, "ash.util.Stream"));
}

static int integral_t(JType *t) {
    JType *u = ty_unbox(t);
    return ty_is_integral(u) || u->kind == TY_CHAR;
}

char *str_of(JType *t, const char *text) {
    if (t->kind == TY_NULL) return "\"null\"";
    if (t->kind == TY_CLASS && ty_is_string(t)) return t->nullable ? j_fmt("__str(%s)", text) : (char *)text;
    JType *u = ty_unbox(t);
    if (!t->nullable) {
        if (u->kind == TY_CHAR) return j_fmt("chr(%s)", text);
        if (u->kind == TY_BOOL) return j_fmt("__bs(%s)", text);
        if (u->kind == TY_FLOAT) return j_fmt("__fstr(%s)", text);
        if (u->kind == TY_DOUBLE) return j_fmt("__dstr(%s)", text);
        if (u->kind == TY_LONG) return j_fmt("__lstr(%s)", text);
        if (ty_is_integral(u)) return j_fmt("str(%s)", text);
    }
    return j_fmt("__str(%s)", text);
}

/* ---------------------------------------------------------------- templates */

int tmpl_uses_receiver(const char *t) { return strstr(t, "$0") != NULL; }

char *expand_tmpl(const char *t, const char *recv, char **args, int nargs) {
    /* "fast||exact": the second form is used with --wrap-int */
    const char *bar = strstr(t, "||");
    if (bar && !strstr(t, " || ")) t = jl_wrap_int ? j_strdup(bar + 2) : j_strndup(t, (size_t)(bar - t));
    JBuf b = {0};
    for (const char *p = t; *p; p++) {
        if (*p == '$' && p[1] >= '0' && p[1] <= '9') {
            int k = p[1] - '0';
            const char *v = k == 0 ? recv : (k <= nargs ? args[k - 1] : "none");
            j_buf_add(&b, v ? v : "none");
            p++;
        } else {
            j_buf_addc(&b, *p);
        }
    }
    return b.data ? b.data : (char *)"";
}

/* ---------------------------------------------------------------- calls */

JMethod *find_ctor(JClass *c, int nparams) {
    jt_complete(c);
    for (int i = 0; i < c->ctors.count; i++) {
        JMethod *k = c->ctors.items[i];
        if (k->params.count == nparams) return k;
    }
    return NULL;
}

static void lx_seq(Node **ns, int cnt, char **out);

static char **lower_args_ex(JMethod *m, JList *args, int *out_n, Node *recv_node, char **recv_text);
static char **lower_args(JMethod *m, JList *args, int *out_n) { return lower_args_ex(m, args, out_n, NULL, NULL); }

static char **lower_args_ex(JMethod *m, JList *args, int *out_n, Node *recv_node, char **recv_text) {
    int np = m ? m->params.count : args->count;
    int n = args->count;
    char **v = j_alloc(sizeof(char *) * (size_t)(n + 2));
    {
        Node *ns[32];
        char *out[32];
        int cnt = 0;
        if (recv_node) ns[cnt++] = recv_node;
        for (int i = 0; i < n && cnt < 32; i++) ns[cnt++] = args->items[i];
        lx_seq(ns, cnt, out);
        int off = 0;
        if (recv_node) { *recv_text = out[0]; off = 1; }
        for (int i = 0; i < n; i++) v[i] = out[i + off];
    }
    if (m && m->varargs) {
        int pack = 1;
        if (n == np) {
            JType *lt = ((Node *)args->items[n - 1])->type;
            if (lt->kind == TY_ARRAY || lt->kind == TY_NULL) pack = 0;
        }
        if (pack) {
            JBuf b = {0};
            j_buf_addc(&b, '[');
            for (int i = np - 1; i < n; i++) {
                if (i > np - 1) j_buf_add(&b, ", ");
                j_buf_add(&b, v[i]);
            }
            j_buf_addc(&b, ']');
            v[np - 1] = b.data;
            n = np;
        }
    }
    *out_n = n;
    return v;
}

static char *join_args(char **a, int n, const char *first) {
    JBuf b = {0};
    int any = 0;
    if (first) { j_buf_add(&b, first); any = 1; }
    for (int i = 0; i < n; i++) {
        if (any) j_buf_add(&b, ", ");
        j_buf_add(&b, a[i]);
        any = 1;
    }
    return b.data ? b.data : (char *)"";
}

static int is_sam_call(JMethod *m, JType *recv) {
    if (!recv || recv->kind != TY_CLASS || m->cls->kind != N_INTERFACE) return 0;
    return jt_find_sam(m->cls) == m && (m->flags & M_ABSTRACT);
}

static int can_devirtualize(JMethod *m) {
    if (m->flags & (M_PRIVATE | M_FINAL | M_STATIC)) return 1;
    if (m->cls->flags & M_FINAL) return 1;
    if (m->cls->kind == N_RECORD || m->cls->kind == N_ENUM) return 1;
    if (m->flags & M_ABSTRACT) return 0;
    if (m->cls->kind == N_INTERFACE) return 0;
    /* no loaded subclass overrides it */
    JList stack = {0};
    j_list_push(&stack, m->cls);
    for (int i = 0; i < stack.count; i++) {
        JClass *c = stack.items[i];
        for (int j = 0; j < c->subclasses.count; j++) {
            JClass *s = c->subclasses.items[j];
            for (int k = 0; k < s->methods.count; k++) {
                JMethod *o = s->methods.items[k];
                if (!strcmp(o->name, m->name) && o->params.count == m->params.count) return 0;
            }
            j_list_push(&stack, s);
        }
    }
    return 1;
}

/* Builds the call text for method `m` on receiver text `recv` (NULL for static). */
static char *invoke(JMethod *m, const char *recv, char **args, int n, JType *recv_type, int super_call, Node *at) {
    if (m->tmpl) return expand_tmpl(m->tmpl, recv, args, n);
    if (m->flags & M_NATIVE) {
        lerr(at, "native method %s.%s has no @Lower template", m->cls->name, m->name);
        return "none";
    }
    if (m->synthetic && m->cls->kind == N_ENUM) {
        if (!strcmp(m->name, "values")) return j_fmt("__enumvalues(%s)", cls_state(m->cls));
        if (!strcmp(m->name, "valueOf")) return j_fmt("__enumvalueof(%s, %s)", cls_state(m->cls), args[0]);
    }
    if (m->is_record_accessor) return j_fmt("%s[%s]", recv, quote_str(m->name));
    if (m->flags & M_STATIC) return j_fmt("%s(%s)", fn_name(m, NULL), join_args(args, n, NULL));
    if (super_call || can_devirtualize(m)) {
        if (m->flags & M_ABSTRACT) {
            lerr(at, "cannot call abstract method %s directly", m->name);
            return "none";
        }
        return j_fmt("%s(%s)", fn_name(m, NULL), join_args(args, n, recv));
    }
    if (n > 6) {
        lerr(at, "calls with more than 6 arguments are not supported by the backend yet");
        return "none";
    }
    if (is_sam_call(m, recv_type)) {
        return j_fmt("__sam%d(%s, %s%s%s)", n, recv, quote_str(method_key(m)), n ? ", " : "", join_args(args, n, NULL));
    }
    return j_fmt("__vc%d(%s, %s%s%s)", n, recv, quote_str(method_key(m)), n ? ", " : "", join_args(args, n, NULL));
}

static char *lower_call(Node *n) {
    JMethod *m = n->sym;
    if (!m) return "none";
    Node *recv_node = NULL;
    JType *rt = NULL;
    char *recv = NULL;
    if (!(m->flags & M_STATIC)) {
        if (n->a) {
            if (n->flags == 1) recv = "self";
            else { recv_node = n->a; rt = n->a->type; }
        } else {
            recv = "self";
            rt = L.cls->self_type;
        }
    } else if (n->a && n->a->rk != R_CLASS && n->a->rk != R_PACKAGE && m->tmpl && tmpl_uses_receiver(m->tmpl)) {
        recv_node = n->a;
    }
    int na;
    char *rtext = NULL;
    char **args = lower_args_ex(m, &n->l1, &na, recv_node, &rtext);
    if (recv_node) recv = rtext;
    return invoke(m, recv, args, na, rt, n->flags == 1, n);
}

static char *lower_new(Node *n) {
    JMethod *k = n->sym;
    if (!k) return "none";
    int na;
    char **args = lower_args(k, &n->l1, &na);
    if (k->tmpl) return expand_tmpl(k->tmpl, NULL, args, na);
    if (k->cls->kind == N_ENUM) {
        lerr(n, "enum constants cannot be created with new");
        return "none";
    }
    char *made = j_fmt("%s(%s)", fn_name(k, "new"), join_args(args, na, NULL));
    JClass *thr = jt_find_class("ash.lang.Throwable");
    if (thr && ty_is_subclass(k->cls, thr)) {
        const char *file = n->path ? strrchr(n->path, '/') : NULL;
        file = file ? file + 1 : (n->path ? n->path : "?");
        const char *where = L.method ? (L.method->is_ctor ? "<init>" : L.method->name) : "<clinit>";
        return j_fmt("__at(%s, %s)", made, quote_str(j_fmt("%s.%s(%s:%d)", L.cls->qname, where, file, n->line)));
    }
    return made;
}

char *lower_ctor_call(Node *n) {
    JMethod *k = n->sym;
    if (!k) return "";
    int na;
    char **args = lower_args(k, &n->l1, &na);
    if (k->tmpl) return "";
    if (!strcmp(n->name, "super")) {
        const char *q = k->cls->qname;
        if (!strcmp(q, "ash.lang.Object") || !strcmp(q, "ash.lang.Enum") || !strcmp(q, "ash.lang.Record")) return "";
    }
    return j_fmt("%s(%s);", fn_name(k, "init"), join_args(args, na, "self"));
}

/* ---------------------------------------------------------------- operators */

static int int_result(JType *lt, JType *rt) {
    JType *r = ty_binary_numeric(lt, rt);
    return r && r->kind == TY_INT;
}

static int is_long_t(JType *t) { return ty_unbox(t)->kind == TY_LONG; }

/* a long operand of a mixed long/float/double operation becomes a double first */
static char *long_to_double(JType *t, const char *v) {
    return is_long_t(t) ? j_fmt("__l2d(%s)", v) : (char *)v;
}

static int float_result(JType *lt, JType *rt) {
    JType *r = ty_binary_numeric(lt, rt);
    return r && r->kind == TY_FLOAT;
}

static char *binop(const char *op, JType *lt, JType *rt, const char *a, const char *b, Node *at) {
    if (jl_wrap_int) {
        JType *num = ty_binary_numeric(lt, rt);
        int cmp = !strcmp(op, "<") || !strcmp(op, "<=") || !strcmp(op, ">") || !strcmp(op, ">=");
        int eq = !strcmp(op, "==") || !strcmp(op, "!=");
        if (num && (num->kind == TY_FLOAT || num->kind == TY_DOUBLE) && (is_long_t(lt) || is_long_t(rt))) {
            a = long_to_double(lt, a);
            b = long_to_double(rt, b);
        } else if (num && num->kind == TY_LONG) {
            if (!strcmp(op, "+")) return j_fmt("__ladd(%s, %s)", a, b);
            if (!strcmp(op, "-")) return j_fmt("__lsub(%s, %s)", a, b);
            if (!strcmp(op, "*")) return j_fmt("__lmul(%s, %s)", a, b);
            if (!strcmp(op, "/")) return j_fmt("__ldiv(%s, %s)", a, b);
            if (!strcmp(op, "%")) return j_fmt("__lrem(%s, %s)", a, b);
            if (!strcmp(op, "&")) return j_fmt("__land(%s, %s)", a, b);
            if (!strcmp(op, "|")) return j_fmt("__lor(%s, %s)", a, b);
            if (!strcmp(op, "^")) return j_fmt("__lxor(%s, %s)", a, b);
            if (cmp) return j_fmt("(__lcmp(%s, %s) %s 0)", a, b, op);
            if (eq) return j_fmt("%s__leq(%s, %s))", op[0] == '!' ? "(not " : "(", a, b);
        }
        if (is_long_t(lt) && !strcmp(op, "<<")) return j_fmt("__lshl(%s, %s)", a, b);
        if (is_long_t(lt) && !strcmp(op, ">>")) return j_fmt("__lshr(%s, %s)", a, b);
        if (is_long_t(lt) && !strcmp(op, ">>>")) return j_fmt("__lushr(%s, %s)", a, b);
        if (!strcmp(op, "==") || !strcmp(op, "!=")) { (void)eq; }
    }
    if (float_result(lt, rt) && (!strcmp(op, "+") || !strcmp(op, "-") || !strcmp(op, "*") || !strcmp(op, "/") || !strcmp(op, "%"))) {
        return j_fmt("__f32(%s %s %s)", a, op, b);   /* every float operation rounds to 24 bits */
    }
    if (jl_wrap_int && int_result(lt, rt)) {
        if (!strcmp(op, "*")) return j_fmt("__imul(%s, %s)", a, b);
        if (!strcmp(op, "+") || !strcmp(op, "-")) return j_fmt("__w(%s %s %s)", a, op, b);
        if (!strcmp(op, "<<")) return j_fmt("__wg(%s << (%s & 31))", a, b);
        if (!strcmp(op, ">>>")) return j_fmt("__wg(__ushr(%s, %s & 31))", a, b);
    }
    if (!strcmp(op, "+") && (ty_is_string(lt) || ty_is_string(rt))) {
        return j_fmt("(%s + %s)", str_of(lt, a), str_of(rt, b));
    }
    if (!strcmp(op, "/") && integral_t(lt) && integral_t(rt)) return j_fmt("__idiv(%s, %s)", a, b);
    if (!strcmp(op, "%") && integral_t(lt) && integral_t(rt)) return j_fmt("__imod(%s, %s)", a, b);
    if (!strcmp(op, "&&")) return j_fmt("(%s and %s)", a, b);
    if (!strcmp(op, "||")) return j_fmt("(%s or %s)", a, b);
    if (ty_unbox(lt)->kind == TY_BOOL && !strcmp(op, "&")) return j_fmt("(%s and %s)", a, b);
    if (ty_unbox(lt)->kind == TY_BOOL && !strcmp(op, "|")) return j_fmt("(%s or %s)", a, b);
    if (ty_unbox(lt)->kind == TY_BOOL && !strcmp(op, "^")) return j_fmt("(%s != %s)", a, b);
    if (!strcmp(op, ">>>")) return j_fmt("__ushr(%s, %s)", a, b);
    (void)at;
    return j_fmt("(%s %s %s)", a, op, b);
}

/* Old Ash only assigns through a plain variable (`a[i] = v`), never through `a[i][j]` or
 * `self["f"][i]`: any other base is hoisted into a temporary first. */
static char *assign_base(Node *base) {
    char *t = lx(base);
    int plain = *t != 0;
    for (const char *p = t; *p; p++) {
        if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '_')) plain = 0;
    }
    if (plain) return t;
    char *tmp = new_temp();
    emit_line(&L.pre, 0, "local %s = %s;", tmp, t);
    return tmp;
}

static char *lvalue(Node *e) {
    switch (e->kind) {
        case N_NAME:
            if (e->rk == R_LOCAL) return sname(e->name);
            if (e->rk == R_FIELD || e->rk == R_THIS_FIELD) return j_fmt("self[%s]", quote_str(e->name));
            if (e->rk == R_STATIC_FIELD || e->rk == R_ENUM_CONST) {
                JField *f = e->sym;
                return j_fmt("%s[%s]", cls_state(f->cls), quote_str(f->name));
            }
            break;
        case N_FIELDACC: {
            JField *f = e->sym;
            if (e->rk == R_STATIC_FIELD || e->rk == R_ENUM_CONST) return j_fmt("%s[%s]", cls_state(f->cls), quote_str(f->name));
            char *base = assign_base(e->a);
            return j_fmt("%s[%s]", base, quote_str(e->name));
        }
        case N_INDEX: {
            char *base = assign_base(e->a);
            char *i = lx(e->b);
            return j_fmt("%s[%s]", base, i);
        }
        default: break;
    }
    lerr(e, "unsupported assignment target");
    return "none";
}


static char *incdec_text(Node *e, const char *cur, int inc);

static int plain_ident(const char *t) {
    if (!*t) return 0;
    for (const char *p = t; *p; p++) {
        if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '_')) return 0;
    }
    return 1;
}

/* Lowers `target = value` operands in Java order: target base/index first, then the value. */
static void assign_parts(Node *e, char **lhs, char **rhs) {
    Node *t = e->a;
    int field = t->kind == N_FIELDACC && t->rk != R_STATIC_FIELD && t->rk != R_ENUM_CONST && t->rk != R_ARRAYLEN;
    if (t->kind == N_INDEX || field) {
        Node *ns[3];
        char *v[3];
        int cnt = 0;
        ns[cnt++] = t->a;
        if (t->kind == N_INDEX) ns[cnt++] = t->b;
        ns[cnt++] = e->b;
        lx_seq(ns, cnt, v);
        char *base = v[0];
        if (!plain_ident(base)) {
            char *tmp = new_temp();
            emit_line(&L.pre, 0, "local %s = %s;", tmp, base);
            base = tmp;
        }
        *lhs = t->kind == N_INDEX ? j_fmt("%s[%s]", base, v[1]) : j_fmt("%s[%s]", base, quote_str(t->name));
        *rhs = v[cnt - 1];
        return;
    }
    *lhs = lvalue(t);
    *rhs = lx(e->b);
}

void emit_assign_stmt(JBuf *b, Node *e, int ind) {
    if (e->kind == N_ASSIGN) {
        char *lhs, *rhs;
        assign_parts(e, &lhs, &rhs);
        if (strcmp(e->name, "=") != 0) {
            char op[4];
            strncpy(op, e->name, 3);
            op[strlen(e->name) - 1] = 0;
            rhs = binop(op, e->a->type, e->b->type, lhs, rhs, e);
        }
        flush_pre(b, ind);
        emit_line(b, ind, "%s = %s;", lhs, rhs);
        return;
    }
    /* ++ / -- as statements */
    Node *t = e->a;
    char *lhs = lvalue(t);
    int inc = e->name[0] == '+';
    flush_pre(b, ind);
    emit_line(b, ind, "%s = %s;", lhs, incdec_text(e, lhs, inc));
}


/* ---------------------------------------------------------------- side effects inside expressions */

/* Assignments, ++ and -- inside expressions become hoisted statements in L.pre. To keep Java's
 * left-to-right order, an operand lowered before a sibling that produced hoisted statements is
 * first saved into a temporary placed *before* them; && || ?: run the hoisted statements of
 * their right-hand side / branches conditionally. */

static int trivial_text(const char *t) {
    if (!*t) return 1;
    if (*t == '"' || (*t >= '0' && *t <= '9')) return 1;
    return !strcmp(t, "yes") || !strcmp(t, "no") || !strcmp(t, "none") || !strcmp(t, "self");
}

static void pre_insert(size_t pos, const char *text) {
    size_t n = strlen(text);
    size_t old = L.pre.len;
    j_buf_addn(&L.pre, text, n);
    memmove(L.pre.data + pos + n, L.pre.data + pos, old - pos);
    memcpy(L.pre.data + pos, text, n);
    L.pre.data[L.pre.len] = 0;
}

static char *pre_cut(size_t pos) {
    char *copy = j_strdup(L.pre.data ? L.pre.data + pos : "");
    L.pre.len = pos;
    if (L.pre.data) L.pre.data[pos] = 0;
    return copy;
}

static void add_indented(const char *text) {
    const char *p = text;
    while (*p) {
        const char *nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);
        j_buf_add(&L.pre, "    ");
        j_buf_addn(&L.pre, p, len);
        j_buf_addc(&L.pre, '\n');
        if (!nl) break;
        p = nl + 1;
    }
}

/* Lowers nodes in order; saves earlier results that a later node's hoisted statements could change. */
static void lx_seq(Node **ns, int cnt, char **out) {
    size_t before[32], after[32];
    for (int i = 0; i < cnt; i++) {
        before[i] = L.pre.len;
        out[i] = lx(ns[i]);
        after[i] = L.pre.len;
    }
    for (int i = cnt - 2; i >= 0; i--) {
        int later = 0;
        for (int j = i + 1; j < cnt; j++) if (after[j] > before[j]) later = 1;
        if (later && !trivial_text(out[i])) {
            char *t = new_temp();
            pre_insert(after[i], j_fmt("local %s = %s;\n", t, out[i]));
            out[i] = t;
        }
    }
}

static char *short_circuit(Node *e) {
    int is_and = e->name[0] == '&';
    char *a = lx(e->a);
    size_t mark = L.pre.len;
    char *b = lx(e->b);
    if (L.pre.len == mark) return j_fmt("(%s %s %s)", a, is_and ? "and" : "or", b);
    char *rhs_pre = pre_cut(mark);
    char *t = new_temp();
    emit_line(&L.pre, 0, "local %s = %s;", t, a);
    emit_line(&L.pre, 0, "given %s%s {", is_and ? "" : "not ", t);
    add_indented(rhs_pre);
    emit_line(&L.pre, 1, "%s = %s;", t, b);
    emit_line(&L.pre, 0, "}");
    return t;
}

static char *conditional(Node *e) {
    char *c = lx(e->a);
    size_t m1 = L.pre.len;
    char *tb = lx(e->b);
    char *pb = pre_cut(m1);
    char *fc = lx(e->c);
    char *pc = pre_cut(m1);
    if (!*pb && !*pc) return j_fmt("(%s ? %s : %s)", c, tb, fc);
    char *t = new_temp();
    emit_line(&L.pre, 0, "local %s = none;", t);
    emit_line(&L.pre, 0, "given %s {", c);
    add_indented(pb);
    emit_line(&L.pre, 1, "%s = %s;", t, tb);
    emit_line(&L.pre, 0, "} otherwise {");
    add_indented(pc);
    emit_line(&L.pre, 1, "%s = %s;", t, fc);
    emit_line(&L.pre, 0, "}");
    return t;
}


/* `x + 1` / `x - 1`, wrapped to 32 bits for int variables unless the for-loop analysis proved it cannot overflow */
static char *incdec_text(Node *e, const char *cur, int inc) {
    JType *t = ty_unbox(e->a->type);
    if (jl_wrap_int && t->kind == TY_LONG) return j_fmt("%s(%s, 1)", inc ? "__ladd" : "__lsub", cur);
    if (jl_wrap_int && t->kind == TY_INT && !(e->flags & 0x200)) return j_fmt("__w(%s %c 1)", cur, inc ? '+' : '-');
    return j_fmt("%s %c 1", cur, inc ? '+' : '-');
}

/* assignment / ++ / -- used as a value: the statement is hoisted, the value is read back (or saved) */
static char *assign_value(Node *e) {
    if (e->kind == N_ASSIGN) {
        char *lhs, *rhs;
        assign_parts(e, &lhs, &rhs);
        if (strcmp(e->name, "=") != 0) {
            char op[4];
            strncpy(op, e->name, 3);
            op[strlen(e->name) - 1] = 0;
            rhs = binop(op, e->a->type, e->b->type, lhs, rhs, e);
        }
        emit_line(&L.pre, 0, "%s = %s;", lhs, rhs);
        return lhs;
    }
    int inc = e->name[0] == '+';
    int prefix = e->kind == N_UNARY;
    char *lv = lvalue(e->a);
    if (prefix) {
        emit_line(&L.pre, 0, "%s = %s;", lv, incdec_text(e, lv, inc));
        return lv;
    }
    char *t = new_temp();
    emit_line(&L.pre, 0, "local %s = %s;", t, lv);
    emit_line(&L.pre, 0, "%s = %s;", lv, incdec_text(e, t, inc));
    return t;
}

/* ---------------------------------------------------------------- main switch */

static char *lower_name(Node *n) {
    switch (n->rk) {
        case R_LOCAL: {
            char *al = alias_get(n->sym);
            return al ? al : sname(n->name);
        }
        case R_FIELD: case R_THIS_FIELD: return j_fmt("self[%s]", quote_str(n->name));
        case R_STATIC_FIELD: case R_ENUM_CONST: {
            JField *f = n->sym;
            if (f->tmpl) return expand_tmpl(f->tmpl, NULL, NULL, 0);
            return j_fmt("%s[%s]", cls_state(f->cls), quote_str(f->name));
        }
        default:
            lerr(n, "cannot lower name '%s'", n->name);
            return "none";
    }
}

static char *lower_fieldacc(Node *n) {
    if (n->rk == R_ARRAYLEN) return j_fmt("len(%s)", lx(n->a));
    JField *f = n->sym;
    if (!f) return "none";
    if (f->tmpl) return expand_tmpl(f->tmpl, NULL, NULL, 0);
    if (n->rk == R_STATIC_FIELD || n->rk == R_ENUM_CONST) return j_fmt("%s[%s]", cls_state(f->cls), quote_str(f->name));
    return j_fmt("%s[%s]", lx(n->a), quote_str(n->name));
}

static char *default_value(JType *t) {
    if (ty_is_prim(t)) return t->kind == TY_BOOL ? "no" : "0";
    return "none";
}

static char *lower_array(Node *n) {
    /* total dims n->flags; dim exprs in l1; optional initializer in b */
    if (n->b) {
        JBuf b = {0};
        j_buf_addc(&b, '[');
        for (int i = 0; i < n->b->l1.count; i++) {
            if (i) j_buf_add(&b, ", ");
            j_buf_add(&b, lx(n->b->l1.items[i]));
        }
        j_buf_addc(&b, ']');
        return b.data;
    }
    JType *t = n->type;
    for (int i = 0; i < n->l1.count; i++) t = t->elem;
    char *dims = NULL;
    JBuf b = {0};
    j_buf_addc(&b, '[');
    for (int i = 0; i < n->l1.count; i++) {
        if (i) j_buf_add(&b, ", ");
        j_buf_add(&b, lx(n->l1.items[i]));
    }
    j_buf_addc(&b, ']');
    dims = b.data;
    return j_fmt("__newarr(%s, %s)", dims, default_value(t));
}

static char *cast_text(Node *n) {
    JType *to = n->type, *from = n->b->type;
    char *v = lx(n->b);
    JType *uf = ty_unbox(from), *ut = ty_unbox(to);
    if (ty_is_prim(ut) && ty_is_prim(uf)) {
        if (jl_wrap_int) {
            if (uf->kind == TY_LONG && ut->kind == TY_INT) return j_fmt("__l2i(%s)", v);
            if (uf->kind == TY_LONG && (ut->kind == TY_BYTE || ut->kind == TY_SHORT || ut->kind == TY_CHAR)) return j_fmt("__wn(__l2i(%s), %d, %d)", v, ut->kind == TY_BYTE ? 8 : 16, ut->kind != TY_CHAR);
            if (uf->kind == TY_LONG && ut->kind == TY_FLOAT) return j_fmt("__f32(__l2d(%s))", v);
            if (uf->kind == TY_LONG && ut->kind == TY_DOUBLE) return j_fmt("__l2d(%s)", v);
            if ((uf->kind == TY_FLOAT || uf->kind == TY_DOUBLE) && ut->kind == TY_LONG) return j_fmt("__d2l(%s)", v);
        }
        if (ut->kind == TY_FLOAT && uf->kind != TY_FLOAT) return j_fmt("__f32(%s)", v);
        int ff = uf->kind == TY_FLOAT || uf->kind == TY_DOUBLE;
        if (ff && ut->kind == TY_INT) return j_fmt("__d2i(%s)", v);          /* saturates, NaN -> 0 */
        if (ff && (ty_is_integral(ut) || ut->kind == TY_CHAR)) {
            if (ut->kind == TY_LONG) return j_fmt("__trunc(%s)", v);
            return j_fmt("__wn(__d2i(%s), %d, %d)", v, ut->kind == TY_BYTE ? 8 : 16, ut->kind != TY_CHAR);
        }
        /* integral narrowing wraps (explicit casts are rare, so this costs nothing in hot code) */
        if (ut->kind == TY_INT && uf->kind == TY_LONG) return j_fmt("__w(__wg(%s))", v);
        if (ut->kind == TY_BYTE && uf->kind != TY_BYTE) return j_fmt("__wn(%s, 8, 1)", v);
        if (ut->kind == TY_SHORT && uf->kind != TY_BYTE && uf->kind != TY_SHORT) return j_fmt("__wn(%s, 16, 1)", v);
        if (ut->kind == TY_CHAR && uf->kind != TY_CHAR) return j_fmt("__wn(%s, 16, 0)", v);
        return v;
    }
    if (to->kind == TY_CLASS && !ty_is_object(to) && !(to->cls->kind == N_INTERFACE && jt_find_sam(to->cls))) {
        return j_fmt("__cast(%s, %s)", v, quote_str(to->cls->qname));
    }
    return v;
}

static char *instance_test(JType *t, const char *v) {
    if (t->kind == TY_CLASS) return j_fmt("__inst(%s, %s)", v, quote_str(t->cls->qname));
    if (t->kind == TY_ARRAY) return j_fmt("(type(%s) == \"array\")", v);
    return "yes";
}

static char *lower_instanceof(Node *n) {
    Node *a = n->a;
    char *v = lx(a);
    if (n->name && n->sym) {
        char *alias = v;
        if (!(a->kind == N_NAME && (a->rk == R_LOCAL || a->rk == R_FIELD)) && a->kind != N_THIS) {
            alias = new_temp();
            emit_line(&L.pre, 0, "local %s = %s;", alias, v);
            v = alias;
        }
        alias_set(n->sym, alias);
    }
    return instance_test(jt_resolve_type(n->b, L.cls, L.method, 0), v);
}

static char *lower_lambda(Node *n) {
    JMethod *sam = n->sym;
    JBuf b = {0};
    j_buf_add(&b, "forge(");
    for (int i = 0; i < n->l1.count; i++) {
        if (i) j_buf_add(&b, ", ");
        j_buf_add(&b, sname(((Node *)n->l1.items[i])->name));
    }
    j_buf_add(&b, ") {");
    char *body = lower_lambda_body(n, sam, 1);
    j_buf_add(&b, body);
    j_buf_add(&b, "}");
    return b.data;
}

static char *lower_methodref(Node *n) {
    JMethod *m = n->sym2;
    JMethod *sam = n->sym;
    if (!m || !sam) return "none";
    int np = sam->params.count;
    char **ps = j_alloc(sizeof(char *) * (size_t)(np + 1));
    for (int i = 0; i < np; i++) ps[i] = j_fmt("__p%d", i);
    char *params = join_args(ps, np, NULL);
    char *call;
    Node *tn = n->a;
    int is_type = tn->rk == R_CLASS;
    if (!strcmp(n->name, "new")) {
        if (m->tmpl) call = expand_tmpl(m->tmpl, NULL, ps, np);
        else call = j_fmt("%s(%s)", fn_name(m, "new"), join_args(ps, np, NULL));
    } else if (m->flags & M_STATIC) {
        call = invoke(m, NULL, ps, np, NULL, 0, n);
    } else if (is_type) {
        call = invoke(m, ps[0], ps + 1, np - 1, tn->type, 0, n);
    } else {
        char *recv = lx(tn);
        if (tn->kind != N_THIS && tn->kind != N_NAME && !(m->tmpl && !tmpl_uses_receiver(m->tmpl))) {
            char *t = new_temp();
            emit_line(&L.pre, 0, "local %s = %s;", t, recv);
            recv = t;
        }
        call = invoke(m, recv, ps, np, tn->type, 0, n);
    }
    if (sam->ret->kind == TY_VOID) {
        return j_fmt("forge(%s) { %s; }", params, call);
    }
    return j_fmt("forge(%s) { yield %s; }", params, call);
}

char *lx(Node *e) {
    switch (e->kind) {
        case N_LIT_INT: return j_fmt("%llu", (unsigned long long)strtoull(e->name, NULL, 0));
        case N_LIT_LONG: {
            unsigned long long v = strtoull(e->name, NULL, 0);
            if (jl_wrap_int && v >= 9007199254740992ULL) {
                long long sv = (long long)v; /* two's complement: 9223372036854775808L is the minimum */
                return j_fmt("{\"__L\": 1, \"hi\": %lld, \"lo\": %llu}", sv >> 32, (unsigned long long)(v & 0xFFFFFFFFULL));
            }
            return j_fmt("%llu", v);
        }
        case N_LIT_DOUBLE: return (char *)e->name;
        case N_LIT_FLOAT: return j_fmt("%.17g", (double)(float)strtod(e->name, NULL));
        case N_LIT_CHAR: return (char *)e->name;
        case N_LIT_STRING: return quote_str(e->name);
        case N_LIT_BOOL: return !strcmp(e->name, "true") ? "yes" : "no";
        case N_LIT_NULL: return "none";
        case N_NAME: return lower_name(e);
        case N_FIELDACC: return lower_fieldacc(e);
        case N_THIS: return "self";
        case N_CALL: return lower_call(e);
        case N_NEW: return lower_new(e);
        case N_NEWARRAY: case N_ARRAYINIT: return lower_array(e);
        case N_INDEX: {
            Node *ns[2] = {e->a, e->b};
            char *v[2];
            lx_seq(ns, 2, v);
            return j_fmt("%s[%s]", v[0], v[1]);
        }
        case N_UNARY:
            if (!strcmp(e->name, "!")) return j_fmt("(not %s)", lx(e->a));
            if (!strcmp(e->name, "++") || !strcmp(e->name, "--")) return assign_value(e);
            if (!strcmp(e->name, "+")) return lx(e->a);
            if (jl_wrap_int && !strcmp(e->name, "-") && ty_unbox(e->a->type)->kind == TY_INT) return j_fmt("__w(-%s)", lx(e->a));
            if (jl_wrap_int && !strcmp(e->name, "-") && is_long_t(e->a->type)) return j_fmt("__lneg(%s)", lx(e->a));
            if (jl_wrap_int && !strcmp(e->name, "~") && is_long_t(e->a->type)) return j_fmt("__lxor(%s, -1)", lx(e->a));
            return j_fmt("(%s%s)", e->name, lx(e->a));
        case N_POSTFIX: return assign_value(e);
        case N_BINARY: {
            if (!strcmp(e->name, "&&") || !strcmp(e->name, "||")) return short_circuit(e);
            Node *ns[2] = {e->a, e->b};
            char *v[2];
            lx_seq(ns, 2, v);
            return binop(e->name, e->a->type, e->b->type, v[0], v[1], e);
        }
        case N_ASSIGN: return assign_value(e);
        case N_COND: return conditional(e);
        case N_CAST: return cast_text(e);
        case N_INSTANCEOF: return lower_instanceof(e);
        case N_LAMBDA: return lower_lambda(e);
        case N_METHODREF: return lower_methodref(e);
        case N_SWITCH: return lower_switch_expr(e);
        default:
            lerr(e, "cannot lower this expression");
            return "none";
    }
}
