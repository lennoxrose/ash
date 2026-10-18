#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "jparse/H/jparse.h"
#include "jtypes/H/jtypes.h"

#ifndef ASH_STDLIB_DIR
#define ASH_STDLIB_DIR "stdlib"
#endif

static JList roots;      /* char* */
static JList all_classes; /* JClass* */
static int method_ids;

void cerr(Node *n, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int len = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    char *msg = j_alloc((size_t)len + 1);
    va_start(ap, fmt);
    vsnprintf(msg, (size_t)len + 1, fmt, ap);
    va_end(ap);
    j_error_soft(n ? n->path : NULL, n ? n->line : 0, n ? n->col : 0, "%s", msg);
}

void jt_add_root(const char *dir) { j_list_push(&roots, j_strdup(dir)); }

static int dir_exists(const char *d) { return access(d, F_OK) == 0; }

void jt_init_default_roots(const char *entry_path, const char *entry_pkg) {
    /* source root = entry file path minus its package directories */
    char *dir = j_strdup(entry_path);
    char *slash = strrchr(dir, '/');
    if (slash) *slash = 0; else strcpy(dir, ".");
    char *root = j_strdup(dir);
    if (entry_pkg && *entry_pkg) {
        char *pk = j_strdup(entry_pkg);
        for (char *c = pk; *c; c++) if (*c == '.') *c = '/';
        size_t dl = strlen(dir), pl = strlen(pk);
        if (dl >= pl && strcmp(dir + dl - pl, pk) == 0 && (dl == pl || dir[dl - pl - 1] == '/')) {
            root = j_strndup(dir, dl > pl ? dl - pl - 1 : 0);
            if (!*root) root = j_strdup(".");
        }
    }
    jt_add_root(root);
    /* dependency roots (forgepack): <root>/ash.libs/<lib>, <root>/@ash-modules/<lib>/src/main/ash */
    const char *env = getenv("ASH_PATH");
    if (env) {
        char *e = j_strdup(env);
        for (char *tok = strtok(e, ":"); tok; tok = strtok(NULL, ":")) jt_add_root(tok);
    }
    const char *std = getenv("ASH_STDLIB");
    jt_add_root(std ? std : ASH_STDLIB_DIR);
    (void)dir_exists;
}

/* ---------------------------------------------------------------- registry */

int jt_all_classes(JClass ***out) {
    *out = (JClass **)all_classes.items;
    return all_classes.count;
}

static JClass *registry_find(const char *qname) {
    for (int i = 0; i < all_classes.count; i++) {
        if (strcmp(((JClass *)all_classes.items[i])->qname, qname) == 0) return all_classes.items[i];
    }
    return NULL;
}


static JClass *declare(Node *unit, Node *d, JClass *outer) {
    JClass *c = j_alloc(sizeof(JClass));
    c->name = d->name;
    c->pkg = unit->name;
    if (outer) c->qname = j_fmt("%s.%s", outer->qname, d->name);
    else c->qname = *unit->name ? j_fmt("%s.%s", unit->name, d->name) : d->name;
    char *m = j_strdup(c->qname);
    for (char *p = m; *p; p++) if (*p == '.') *p = '_';
    c->mangled = m;
    c->kind = d->kind;
    c->flags = d->flags;
    c->decl = d;
    c->path = d->path;
    c->unit = unit;
    c->outer = outer;
    c->order = all_classes.count;
    if (registry_find(c->qname)) {
        cerr(d, "duplicate definition of type '%s'", c->qname);
        return c;
    }
    d->sym = c;
    j_list_push(&all_classes, c);
    if (outer) {
        j_list_push(&outer->nested, c);
        c->flags |= M_STATIC;
        if (outer->kind == N_INTERFACE) c->flags |= M_PUBLIC;
    }
    for (int i = 0; i < d->l1.count; i++) {
        Node *mem = d->l1.items[i];
        if (mem->kind == N_CLASS || mem->kind == N_INTERFACE || mem->kind == N_ENUM || mem->kind == N_RECORD) {
            declare(unit, mem, c);
        }
    }
    return c;
}

JClass *jt_load_unit(const char *path, const char *src) {
    Node *u = jparse_unit(path, src);
    JClass *first = NULL;
    for (int i = 0; i < u->l2.count; i++) {
        JClass *c = declare(u, u->l2.items[i], NULL);
        if (!first) first = c;
    }
    return first;
}

static int path_has_decl(const char *file, const char *simple) { (void)file; (void)simple; return 1; }

JClass *jt_find_class(const char *qname) {
    JClass *c = registry_find(qname);
    if (c) return c;
    /* try to load <root>/<a/b/C>.ash; for nested names load the outer file */
    char *copy = j_strdup(qname);
    for (;;) {
        char *file = j_strdup(copy);
        for (char *p = file; *p; p++) if (*p == '.') *p = '/';
        for (int i = 0; i < roots.count; i++) {
            char *full = j_fmt("%s/%s.ash", (char *)roots.items[i], file);
            char *src = j_read_file(full);
            if (src && path_has_decl(full, copy)) {
                jt_load_unit(full, src);
                c = registry_find(qname);
                if (c) return c;
            }
        }
        char *dot = strrchr(copy, '.');
        if (!dot) return NULL;
        *dot = 0;
    }
}

JClass *jt_well_known(const char *qname) {
    JClass *c = jt_find_class(qname);
    if (!c) {
        fprintf(stderr, "error: standard library type '%s' not found (looked in the Ash stdlib; set ASH_STDLIB)\n", qname);
        exit(1);
    }
    return c;
}

static JClass *object_cls, *string_cls;
JClass *jt_object(void) {
    if (!object_cls) object_cls = jt_well_known("ash.lang.Object");
    return object_cls;
}
JClass *jt_string(void) {
    if (!string_cls) string_cls = jt_well_known("ash.lang.String");
    return string_cls;
}
JType *jt_string_type(void) { jt_complete(jt_string()); return jt_string()->self_type; }
JType *jt_object_type(void) { jt_complete(jt_object()); return jt_object()->self_type; }

/* ---------------------------------------------------------------- type resolution */

static JClass *find_nested(JClass *c, const char *name, int depth) {
    if (!c || depth > 8) return NULL;
    for (int i = 0; i < c->nested.count; i++) {
        JClass *n = c->nested.items[i];
        if (strcmp(n->name, name) == 0) return n;
    }
    jt_complete(c);
    if (c->super) {
        JClass *r = find_nested(c->super->cls, name, depth + 1);
        if (r) return r;
    }
    for (int i = 0; i < c->ifaces.count; i++) {
        JClass *r = find_nested(((JType *)c->ifaces.items[i])->cls, name, depth + 1);
        if (r) return r;
    }
    return NULL;
}

static JClass *resolve_class_simple(const char *name, JClass *ctx) {
    for (JClass *c = ctx; c; c = c->outer) {
        if (strcmp(c->name, name) == 0) return c;
        JClass *n = find_nested(c, name, 0);
        if (n) return n;
    }
    Node *unit = ctx ? ctx->unit : NULL;
    if (unit) {
        const char *pkg = unit->name;
        JClass *c = jt_find_class(*pkg ? j_fmt("%s.%s", pkg, name) : name);
        if (c) return c;
        for (int i = 0; i < unit->l1.count; i++) {
            Node *im = unit->l1.items[i];
            if (!im->flags) {
                const char *last = strrchr(im->name, '.');
                last = last ? last + 1 : im->name;
                if (strcmp(last, name) == 0) {
                    c = jt_find_class(im->name);
                    if (c) return c;
                }
            }
        }
        for (int i = 0; i < unit->l1.count; i++) {
            Node *im = unit->l1.items[i];
            if (im->flags) {
                c = jt_find_class(j_fmt("%s.%s", im->name, name));
                if (c) return c;
            }
        }
    }
    JClass *c = jt_find_class(j_fmt("ash.lang.%s", name));
    if (c) return c;
    return jt_find_class(j_fmt("ash.lang.function.%s", name));
}

static JTVar *find_tvar(const char *name, JClass *ctx, JMethod *mctx) {
    if (mctx) {
        for (int i = 0; i < mctx->tparams.count; i++) {
            JTVar *tv = mctx->tparams.items[i];
            if (strcmp(tv->name, name) == 0) return tv;
        }
    }
    for (JClass *c = ctx; c; c = c->outer) {
        for (int i = 0; i < c->tparams.count; i++) {
            JTVar *tv = c->tparams.items[i];
            if (strcmp(tv->name, name) == 0) return tv;
        }
    }
    return NULL;
}

static JType *prim_by_name(const char *n) {
    static const struct { const char *n; TyKind k; } tbl[] = {
        {"void", TY_VOID}, {"bool", TY_BOOL}, {"byte", TY_BYTE}, {"short", TY_SHORT},
        {"char", TY_CHAR}, {"int", TY_INT}, {"long", TY_LONG}, {"float", TY_FLOAT}, {"double", TY_DOUBLE},
    };
    for (size_t i = 0; i < sizeof(tbl) / sizeof(tbl[0]); i++) {
        if (strcmp(tbl[i].n, n) == 0) return ty_prim(tbl[i].k);
    }
    return NULL;
}

JType *jt_resolve_type(Node *tn, JClass *ctx, JMethod *mctx, int report) {
    JType *base = prim_by_name(tn->name);
    if (base) {
        for (int i = 0; i < tn->flags; i++) base = ty_array(base);
        if (tn->nullable) {
            if (ty_is_prim(base)) {
                if (report) cerr(tn, "primitive type '%s' cannot be nullable; use the boxed type", ty_str(base));
            } else {
                base = ty_with_null(base, 1);
            }
        }
        return base;
    }
    int dims = tn->flags & 0xFFFF;
    JType *t = NULL;
    if (!strchr(tn->name, '.')) {
        JTVar *tv = find_tvar(tn->name, ctx, mctx);
        if (tv) t = ty_tvar(tv);
    }
    if (!t) {
        JClass *c = NULL;
        char *copy = j_strdup(tn->name);
        char *dot = strchr(copy, '.');
        if (!dot) {
            c = resolve_class_simple(copy, ctx);
        } else {
            *dot = 0;
            JClass *outer = resolve_class_simple(copy, ctx);
            if (outer) {
                c = outer;
                char *rest = dot + 1;
                for (char *seg = strtok(rest, "."); seg && c; seg = strtok(NULL, ".")) c = find_nested(c, seg, 0);
            }
            if (!c) c = jt_find_class(tn->name);
        }
        if (!c) {
            if (report) cerr(tn, "cannot find type '%s'", tn->name);
            return ty_error();
        }
        JType **args = NULL;
        int nargs = tn->l1.count;
        if (nargs) {
            args = j_alloc(sizeof(JType *) * (size_t)nargs);
            for (int i = 0; i < nargs; i++) {
                Node *an = tn->l1.items[i];
                if (an->kind == N_WILDCARD) {
                    args[i] = an->a ? jt_resolve_type(an->a, ctx, mctx, report) : jt_object_type();
                } else {
                    args[i] = jt_resolve_type(an, ctx, mctx, report);
                    if (ty_is_prim(args[i]) && report) {
                        cerr(an, "type arguments must be reference types; use '%s' instead of '%s'",
                             ty_str(ty_box(args[i])), ty_str(args[i]));
                    }
                }
            }
        }
        t = ty_class(c, args, nargs);
    }
    for (int i = 0; i < dims; i++) t = ty_array(t);
    if (tn->nullable) t = ty_with_null(t, 1);
    return t;
}

/* ---------------------------------------------------------------- headers and members */

static const char *anno_str(JList *annos, const char *name, int *present) {
    for (int i = 0; i < annos->count; i++) {
        Node *a = annos->items[i];
        if (strcmp(a->name, name) == 0) {
            if (present) *present = 1;
            return a->str;
        }
    }
    return NULL;
}

static int has_anno(JList *annos, const char *name) {
    int p = 0;
    anno_str(annos, name, &p);
    return p;
}

static JMethod *new_method(JClass *c, const char *name, int flags, JType *ret) {
    JMethod *m = j_alloc(sizeof(JMethod));
    m->name = name;
    m->flags = flags;
    m->cls = c;
    m->ret = ret;
    m->id = method_ids++;
    return m;
}

static void add_param(JMethod *m, const char *name, JType *t) {
    JParam *p = j_alloc(sizeof(JParam));
    p->name = name;
    p->type = t;
    j_list_push(&m->params, p);
}

static JField *add_field(JClass *c, const char *name, JType *t, int flags, Node *decl) {
    JField *f = j_alloc(sizeof(JField));
    f->name = name;
    f->type = t;
    f->flags = flags;
    f->cls = c;
    f->decl = decl;
    f->ordinal = c->fields.count;
    j_list_push(&c->fields, f);
    return f;
}

static int has_method_sig(JClass *c, const char *name, int nparams) {
    for (int i = 0; i < c->methods.count; i++) {
        JMethod *m = c->methods.items[i];
        if (strcmp(m->name, name) == 0 && m->params.count == nparams) return 1;
    }
    return 0;
}

static void member_field(JClass *c, Node *f) {
    JType *t = jt_resolve_type(f->a, c, NULL, 1);
    int flags = f->flags;
    if (c->kind == N_INTERFACE) flags |= M_PUBLIC | M_STATIC | M_FINAL;
    if (!(flags & (M_PUBLIC | M_PROTECTED | M_PRIVATE))) flags |= M_PRIVATE; /* D7 */
    if ((flags & M_PUBLIC) && !(flags & M_FINAL) && !has_anno(&f->l1, "PublicField") && c->kind != N_INTERFACE &&
        !(c->flags & 0x40000)) {
        cerr(f, "public mutable field '%s' is not allowed; make it private, final, or annotate it with @PublicField", f->name);
    }
    JField *jf = add_field(c, f->name, t, flags, f);
    jf->tmpl = anno_str(&f->l1, "Lower", NULL);
    for (int i = 0; i < c->fields.count - 1; i++) {
        if (strcmp(((JField *)c->fields.items[i])->name, f->name) == 0) cerr(f, "duplicate field '%s'", f->name);
    }
}

static void member_method(JClass *c, Node *m) {
    int flags = m->flags;
    JMethod *jm = new_method(c, m->name, flags, NULL);
    jm->decl = m;
    jm->is_ctor = m->kind == N_CTOR;
    for (int i = 0; i < m->l2.count; i++) {
        Node *tp = m->l2.items[i];
        JTVar *tv = j_alloc(sizeof(JTVar));
        tv->name = tp->name;
        tv->owner_m = jm;
        j_list_push(&jm->tparams, tv);
    }
    for (int i = 0; i < m->l2.count; i++) {
        Node *tp = m->l2.items[i];
        if (tp->a) ((JTVar *)jm->tparams.items[i])->bound = jt_resolve_type(tp->a, c, jm, 1);
    }
    if (c->kind == N_INTERFACE) {
        if (!(flags & (M_PRIVATE))) jm->flags |= M_PUBLIC;
        if (!m->b && !(flags & (M_STATIC | M_NATIVE))) jm->flags |= M_ABSTRACT;
    }
    jm->ret = m->a ? jt_resolve_type(m->a, c, jm, 1) : ty_prim(TY_VOID);
    if (m->flags & 0x20000) { /* compact canonical record constructor: params = components */
        for (int i = 0; i < c->decl->l4.count; i++) {
            Node *rc = c->decl->l4.items[i];
            add_param(jm, rc->name, jt_resolve_type(rc->a, c, jm, 1));
        }
    }
    for (int i = 0; i < m->l1.count; i++) {
        Node *p = m->l1.items[i];
        add_param(jm, p->name, jt_resolve_type(p->a, c, jm, 1));
        if (p->flags & M_VARARGS) jm->varargs = 1;
    }
    for (int i = 0; i < m->l3.count; i++) {
        j_list_push(&jm->throws, jt_resolve_type(m->l3.items[i], c, jm, 1));
    }
    jm->tmpl = anno_str(&m->l4, "Lower", NULL);
    jm->has_override_anno = has_anno(&m->l4, "Override");
    if ((jm->flags & M_ABSTRACT) && m->b) cerr(m, "abstract method '%s' cannot have a body", m->name);
    if (!(jm->flags & (M_ABSTRACT | M_NATIVE)) && !m->b && !jm->tmpl) {
        cerr(m, "method '%s' needs a body (or declare it abstract/native)", m->name);
    }
    if (jm->is_ctor) j_list_push(&c->ctors, jm);
    else j_list_push(&c->methods, jm);
    m->sym = jm;
}

static void synth_record(JClass *c) {
    Node *d = c->decl;
    JMethod *canon = NULL;
    for (int i = 0; i < d->l4.count; i++) {
        Node *rc = d->l4.items[i];
        JType *t = jt_resolve_type(rc->a, c, NULL, 1);
        add_field(c, rc->name, t, M_PRIVATE | M_FINAL, rc);
    }
    for (int i = 0; i < c->ctors.count; i++) {
        JMethod *k = c->ctors.items[i];
        if (k->params.count == d->l4.count) {
            int same = 1;
            for (int j = 0; j < k->params.count; j++) {
                JField *f = c->fields.items[j];
                if (!ty_same(((JParam *)k->params.items[j])->type, f->type)) same = 0;
            }
            if (same) canon = k;
        }
    }
    if (!canon) {
        canon = new_method(c, c->name, M_PUBLIC, ty_prim(TY_VOID));
        canon->is_ctor = 1;
        canon->synthetic = 1;
        for (int i = 0; i < d->l4.count; i++) {
            JField *f = c->fields.items[i];
            add_param(canon, f->name, f->type);
        }
        j_list_push(&c->ctors, canon);
    }
    for (int i = 0; i < d->l4.count; i++) {
        JField *f = c->fields.items[i];
        if (!has_method_sig(c, f->name, 0)) {
            JMethod *acc = new_method(c, f->name, M_PUBLIC, f->type);
            acc->synthetic = 1;
            acc->is_record_accessor = 1;
            j_list_push(&c->methods, acc);
        }
    }
    JType *str = jt_string_type();
    if (!has_method_sig(c, "toString", 0)) {
        JMethod *m = new_method(c, "toString", M_PUBLIC, str);
        m->synthetic = 1;
        j_list_push(&c->methods, m);
    }
    if (!has_method_sig(c, "hashCode", 0)) {
        JMethod *m = new_method(c, "hashCode", M_PUBLIC, ty_prim(TY_INT));
        m->synthetic = 1;
        j_list_push(&c->methods, m);
    }
    if (!has_method_sig(c, "equals", 1)) {
        JMethod *m = new_method(c, "equals", M_PUBLIC, ty_prim(TY_BOOL));
        m->synthetic = 1;
        add_param(m, "o", jt_object_type());
        j_list_push(&c->methods, m);
    }
}

static void synth_enum(JClass *c) {
    Node *d = c->decl;
    JType *self = ty_class(c, NULL, 0);
    for (int i = 0; i < d->l4.count; i++) {
        Node *ec = d->l4.items[i];
        JField *f = add_field(c, ec->name, self, M_PUBLIC | M_STATIC | M_FINAL, ec);
        f->is_enum_const = 1;
        j_list_push(&c->enum_names, (void *)ec->name);
    }
    JMethod *values = new_method(c, "values", M_PUBLIC | M_STATIC, ty_array(self));
    values->synthetic = 1;
    j_list_push(&c->methods, values);
    JMethod *vo = new_method(c, "valueOf", M_PUBLIC | M_STATIC, self);
    vo->synthetic = 1;
    add_param(vo, "name", jt_string_type());
    j_list_push(&c->methods, vo);
}

void jt_complete(JClass *c) {
    if (c->header_done) return;
    c->header_done = 1;
    Node *d = c->decl;
    /* type parameters */
    JType **targs = NULL;
    if (d->l2.count) targs = j_alloc(sizeof(JType *) * (size_t)d->l2.count);
    for (int i = 0; i < d->l2.count; i++) {
        Node *tp = d->l2.items[i];
        JTVar *tv = j_alloc(sizeof(JTVar));
        tv->name = tp->name;
        tv->owner_cls = c;
        j_list_push(&c->tparams, tv);
        targs[i] = ty_tvar(tv);
    }
    c->self_type = ty_class(c, targs, d->l2.count);
    for (int i = 0; i < d->l2.count; i++) {
        Node *tp = d->l2.items[i];
        if (tp->a) ((JTVar *)c->tparams.items[i])->bound = jt_resolve_type(tp->a, c, NULL, 1);
    }
    /* supertypes */
    int is_object = strcmp(c->qname, "ash.lang.Object") == 0;
    if (c->kind == N_INTERFACE) {
        for (int i = 0; i < d->l3.count; i++) j_list_push(&c->ifaces, jt_resolve_type(d->l3.items[i], c, NULL, 1));
    } else {
        if (d->a) {
            c->super = jt_resolve_type(d->a, c, NULL, 1);
            if (c->super->kind == TY_CLASS) {
                JClass *sc = c->super->cls;
                jt_complete(sc);
                if (sc->kind == N_INTERFACE) cerr(d->a, "class '%s' cannot extend interface '%s'; use implements", c->name, sc->name);
                else if (sc->kind == N_ENUM || sc->kind == N_RECORD) cerr(d->a, "cannot extend %s '%s'", sc->kind == N_ENUM ? "enum" : "record", sc->name);
                else if (sc->flags & M_FINAL) cerr(d->a, "cannot extend final class '%s'", sc->name);
            } else {
                c->super = NULL;
            }
        } else if (c->kind == N_ENUM) {
            JClass *e = jt_well_known("ash.lang.Enum");
            JType **a = j_alloc(sizeof(JType *));
            a[0] = ty_class(c, NULL, 0);
            c->super = ty_class(e, a, 1);
        } else if (c->kind == N_RECORD) {
            c->super = jt_well_known("ash.lang.Record")->self_type;
        } else if (!is_object) {
            c->super = jt_object_type();
        }
        for (int i = 0; i < d->l3.count; i++) {
            JType *it = jt_resolve_type(d->l3.items[i], c, NULL, 1);
            if (it->kind == TY_CLASS) {
                jt_complete(it->cls);
                if (it->cls->kind != N_INTERFACE) {
                    cerr(d->l3.items[i], "'%s' is not an interface; classes use extends for superclasses", it->cls->name);
                    continue;
                }
            }
            j_list_push(&c->ifaces, it);
        }
    }
    if (c->super && c->super->kind == TY_CLASS) j_list_push(&c->super->cls->subclasses, c);
    for (int i = 0; i < c->ifaces.count; i++) {
        JType *it = c->ifaces.items[i];
        if (it->kind == TY_CLASS) j_list_push(&it->cls->subclasses, c);
    }
    for (int i = 0; i < d->l5.count; i++) {
        JType *pt = jt_resolve_type(d->l5.items[i], c, NULL, 1);
        if (pt->kind == TY_CLASS) j_list_push(&c->permits, pt->cls);
    }
    /* members */
    if (c->kind == N_ENUM) synth_enum(c);
    if (c->kind == N_RECORD) {
        for (int i = 0; i < d->l1.count; i++) {
            Node *mem = d->l1.items[i];
            if (mem->kind == N_CTOR) member_method(c, mem);
        }
    }
    for (int i = 0; i < d->l1.count; i++) {
        Node *mem = d->l1.items[i];
        if (c->kind == N_RECORD && mem->kind == N_CTOR) continue;
        if (mem->kind == N_FIELD) member_field(c, mem);
        else if (mem->kind == N_SEQ) {
            for (int j = 0; j < mem->l1.count; j++) member_field(c, mem->l1.items[j]);
        } else if (mem->kind == N_METHOD || mem->kind == N_CTOR) member_method(c, mem);
    }
    if (c->kind == N_RECORD) synth_record(c);
    if (c->kind != N_INTERFACE && c->ctors.count == 0) {
        JMethod *k = new_method(c, c->name, c->kind == N_ENUM ? M_PRIVATE : (c->flags & (M_PUBLIC | M_PROTECTED | M_PRIVATE)), ty_prim(TY_VOID));
        k->is_ctor = 1;
        k->synthetic = 1;
        j_list_push(&c->ctors, k);
    }
    c->members_done = 1;
}

/* ---------------------------------------------------------------- member lookup */

JField *jt_find_field(JClass *c, const char *name) {
    jt_complete(c);
    for (int i = 0; i < c->fields.count; i++) {
        JField *f = c->fields.items[i];
        if (strcmp(f->name, name) == 0) return f;
    }
    if (c->super) {
        JField *f = jt_find_field(c->super->cls, name);
        if (f) return f;
    }
    for (int i = 0; i < c->ifaces.count; i++) {
        JField *f = jt_find_field(((JType *)c->ifaces.items[i])->cls, name);
        if (f) return f;
    }
    return NULL;
}

static int same_erased_params(JMethod *a, JMethod *b) {
    if (a->params.count != b->params.count) return 0;
    for (int i = 0; i < a->params.count; i++) {
        JType *x = ty_erase_bound(((JParam *)a->params.items[i])->type);
        JType *y = ty_erase_bound(((JParam *)b->params.items[i])->type);
        if (x->kind != y->kind) {
            if (!(x->kind == TY_CLASS && y->kind == TY_CLASS)) return 0;
        }
        if (x->kind == TY_CLASS && y->kind == TY_CLASS) {
            /* a type variable on one side is treated as matching any reference type: it may be
             * the implementation of a generic parameter (e.g. compareTo(T) vs compareTo(Point)) */
            JType *xp = ((JParam *)a->params.items[i])->type, *yp = ((JParam *)b->params.items[i])->type;
            if (xp->kind == TY_TVAR || yp->kind == TY_TVAR) continue;
            if (x->cls != y->cls) return 0;
        }
        if (x->kind == TY_ARRAY && !ty_same(x, y)) return 0;
        if (ty_is_prim(x) && x->kind != y->kind) return 0;
    }
    return 1;
}

static void collect(JClass *c, const char *name, JList *out, int depth) {
    if (depth > 16) return;
    jt_complete(c);
    for (int i = 0; i < c->methods.count; i++) {
        JMethod *m = c->methods.items[i];
        if (strcmp(m->name, name) != 0) continue;
        int dup = 0;
        for (int j = 0; j < out->count; j++) {
            if (same_erased_params(out->items[j], m)) { dup = 1; break; }
        }
        if (!dup) j_list_push(out, m);
    }
    if (c->super) collect(c->super->cls, name, out, depth + 1);
    for (int i = 0; i < c->ifaces.count; i++) collect(((JType *)c->ifaces.items[i])->cls, name, out, depth + 1);
}

void jt_collect_methods(JClass *c, const char *name, JList *out) {
    collect(c, name, out, 0);
    if (c->kind == N_INTERFACE) collect(jt_object(), name, out, 0);
}

static void collect_abstract(JClass *c, JList *out, int depth) {
    if (depth > 16) return;
    jt_complete(c);
    for (int i = 0; i < c->methods.count; i++) {
        JMethod *m = c->methods.items[i];
        if (!(m->flags & M_ABSTRACT) || (m->flags & M_STATIC)) continue;
        if (!strcmp(m->name, "equals") || !strcmp(m->name, "hashCode") || !strcmp(m->name, "toString")) continue;
        int dup = 0;
        for (int j = 0; j < out->count; j++) {
            JMethod *o = out->items[j];
            if (!strcmp(o->name, m->name) && o->params.count == m->params.count) dup = 1;
        }
        if (!dup) j_list_push(out, m);
    }
    for (int i = 0; i < c->ifaces.count; i++) collect_abstract(((JType *)c->ifaces.items[i])->cls, out, depth + 1);
}

JMethod *jt_find_sam(JClass *c) {
    if (c->sam_done) return c->sam;
    c->sam_done = 1;
    if (c->kind != N_INTERFACE) return NULL;
    JList abs = {0};
    collect_abstract(c, &abs, 0);
    /* an abstract method re-declared with a default body in a sub-interface is not abstract */
    JList real = {0};
    for (int i = 0; i < abs.count; i++) {
        JMethod *m = abs.items[i];
        JList all = {0};
        jt_collect_methods(c, m->name, &all);
        int implemented = 0;
        for (int j = 0; j < all.count; j++) {
            JMethod *o = all.items[j];
            if (!(o->flags & M_ABSTRACT) && !(o->flags & M_STATIC) && o->params.count == m->params.count &&
                o->cls != jt_object()) implemented = 1;
        }
        if (!implemented) j_list_push(&real, m);
    }
    if (real.count == 1) c->sam = real.items[0];
    return c->sam;
}

int jt_is_functional(JType *t) {
    return t->kind == TY_CLASS && t->cls->kind == N_INTERFACE && jt_find_sam(t->cls) != NULL;
}
