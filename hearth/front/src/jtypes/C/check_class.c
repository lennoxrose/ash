#include <stdlib.h>
#include <string.h>

#include "jtypes/H/check.h"

/* Class-level checking: inheritance, overrides, abstract implementation, sealed
 * hierarchies, then every method, constructor, field initializer and enum constant. */

#define TVOID ty_prim(TY_VOID)

static int access_rank(int flags) {
    if (flags & M_PUBLIC) return 3;
    if (flags & M_PROTECTED) return 2;
    if (flags & M_PRIVATE) return 0;
    return 1;
}

static const char *access_name(int flags) {
    return (flags & M_PUBLIC) ? "public" : (flags & M_PROTECTED) ? "protected" : (flags & M_PRIVATE) ? "private" : "package-private";
}

static int same_params(JMethod *a, JMethod *b) {
    if (a->params.count != b->params.count) return 0;
    for (int i = 0; i < a->params.count; i++) {
        JType *x = ((JParam *)a->params.items[i])->type, *y = ((JParam *)b->params.items[i])->type;
        if (x->kind == TY_TVAR || y->kind == TY_TVAR) continue;
        x = ty_erase_bound(x);
        y = ty_erase_bound(y);
        if (x->kind != y->kind) return 0;
        if (x->kind == TY_CLASS && x->cls != y->cls) return 0;
        if (x->kind == TY_ARRAY && !ty_same(x, y)) return 0;
    }
    return 1;
}

static JMethod *find_in(JClass *c, JMethod *m, int depth) {
    if (!c || depth > 16) return NULL;
    jt_complete(c);
    for (int i = 0; i < c->methods.count; i++) {
        JMethod *o = c->methods.items[i];
        if (o != m && !strcmp(o->name, m->name) && !(o->flags & M_PRIVATE) && same_params(o, m)) return o;
    }
    if (c->super) {
        JMethod *r = find_in(c->super->cls, m, depth + 1);
        if (r) return r;
    }
    for (int i = 0; i < c->ifaces.count; i++) {
        JMethod *r = find_in(((JType *)c->ifaces.items[i])->cls, m, depth + 1);
        if (r) return r;
    }
    return NULL;
}

JMethod *jt_find_overridden(JClass *c, JMethod *m) {
    JMethod *r = NULL;
    if (c->super) r = find_in(c->super->cls, m, 0);
    for (int i = 0; i < c->ifaces.count && !r; i++) r = find_in(((JType *)c->ifaces.items[i])->cls, m, 0);
    if (!r && c->kind == N_INTERFACE) r = find_in(jt_object(), m, 0);
    return r;
}

static void check_overrides(JClass *c) {
    for (int i = 0; i < c->methods.count; i++) {
        JMethod *m = c->methods.items[i];
        Node *at = m->decl ? m->decl : c->decl;
        for (int j = 0; j < i; j++) {
            JMethod *o = c->methods.items[j];
            if (!strcmp(o->name, m->name) && same_params(o, m) && o->params.count == m->params.count && !m->synthetic && !o->synthetic) {
                int same = 1;
                for (int k = 0; k < m->params.count; k++) {
                    if (!ty_same(((JParam *)m->params.items[k])->type, ((JParam *)o->params.items[k])->type)) same = 0;
                }
                if (same) cerr(at, "method '%s' is already defined in %s", m->name, c->name);
            }
        }
        if (m->flags & M_STATIC) {
            if (m->has_override_anno) cerr(at, "static method '%s' cannot have @Override", m->name);
            continue;
        }
        JMethod *o = jt_find_overridden(c, m);
        if (m->has_override_anno && !o) {
            cerr(at, "method '%s' is annotated @Override but does not override or implement a method of a supertype", m->name);
            continue;
        }
        if (!o || (o->flags & M_STATIC)) continue;
        if (o->flags & M_FINAL) cerr(at, "method '%s' cannot override final method of %s", m->name, o->cls->name);
        if (access_rank(m->flags) < access_rank(o->flags)) {
            cerr(at, "method '%s' cannot reduce visibility from %s (in %s) to %s", m->name, access_name(o->flags), o->cls->name, access_name(m->flags));
        }
        JType *oret = member_type(c->self_type, o->cls, o->ret);
        int ok = ty_is_prim(m->ret) || m->ret->kind == TY_VOID || ty_is_prim(oret) || oret->kind == TY_VOID
                     ? (m->ret->kind == oret->kind)
                     : ty_assignable(m->ret, ty_with_null(oret, 1), 1) || oret->kind == TY_TVAR;
        if (!ok) cerr(at, "return type %s of '%s' is not compatible with %s in %s", ty_str(m->ret), m->name, ty_str(oret), o->cls->name);
        for (int k = 0; k < m->throws.count; k++) {
            JType *t = m->throws.items[k];
            if (!ty_is_checked_exception(t)) continue;
            int allowed = 0;
            for (int q = 0; q < o->throws.count; q++) {
                JType *ot = o->throws.items[q];
                if (t->kind == TY_CLASS && ot->kind == TY_CLASS && ty_is_subclass(t->cls, ot->cls)) allowed = 1;
            }
            if (!allowed) cerr(at, "'%s' cannot throw %s: the overridden method in %s does not declare it", m->name, ty_str(t), o->cls->name);
        }
    }
}

static void collect_abstract_all(JClass *c, JList *out, int depth) {
    if (!c || depth > 16) return;
    jt_complete(c);
    for (int i = 0; i < c->methods.count; i++) {
        JMethod *m = c->methods.items[i];
        if ((m->flags & M_ABSTRACT) && !(m->flags & M_STATIC)) j_list_push(out, m);
    }
    if (c->super) collect_abstract_all(c->super->cls, out, depth + 1);
    for (int i = 0; i < c->ifaces.count; i++) collect_abstract_all(((JType *)c->ifaces.items[i])->cls, out, depth + 1);
}

static void check_abstract_impl(JClass *c) {
    if (c->kind == N_INTERFACE || (c->flags & M_ABSTRACT)) return;
    JList abs = {0};
    collect_abstract_all(c, &abs, 0);
    for (int i = 0; i < abs.count; i++) {
        JMethod *am = abs.items[i];
        JList all = {0};
        jt_collect_methods(c, am->name, &all);
        int impl = 0;
        for (int j = 0; j < all.count; j++) {
            JMethod *o = all.items[j];
            if (!(o->flags & M_ABSTRACT) && !(o->flags & M_STATIC) && o->params.count == am->params.count) impl = 1;
        }
        if (!impl) {
            cerr(c->decl, "%s '%s' must implement abstract method '%s' of %s (or be declared abstract)",
                 c->kind == N_ENUM ? "enum" : c->kind == N_RECORD ? "record" : "class", c->name, am->name, am->cls->name);
            return;
        }
    }
}

static void check_sealed(JClass *c) {
    JList supers = {0};
    if (c->super) j_list_push(&supers, c->super);
    for (int i = 0; i < c->ifaces.count; i++) j_list_push(&supers, c->ifaces.items[i]);
    for (int i = 0; i < supers.count; i++) {
        JType *st = supers.items[i];
        if (st->kind != TY_CLASS || !(st->cls->flags & M_SEALED)) continue;
        JClass *s = st->cls;
        jt_complete(s);
        if (s->permits.count) {
            int listed = 0;
            for (int j = 0; j < s->permits.count; j++) if (s->permits.items[j] == c) listed = 1;
            if (!listed) cerr(c->decl, "%s '%s' is not listed in the permits clause of sealed type %s", c->kind == N_INTERFACE ? "interface" : "class", c->name, s->name);
        } else if (strcmp(s->unit->name, c->unit->name) != 0 || s->path != c->path) {
            cerr(c->decl, "sealed type %s without a permits clause can only be extended in its own file", s->name);
        }
        if (c->kind == N_CLASS && !(c->flags & (M_FINAL | M_SEALED))) {
            cerr(c->decl, "class '%s' extends sealed %s and must be declared final or sealed", c->name, s->name);
        }
        if (c->kind == N_INTERFACE && !(c->flags & M_SEALED)) {
            /* interfaces cannot be final; allow sub-interfaces to stay open only when sealed */
            cerr(c->decl, "interface '%s' extends sealed %s and must be declared sealed", c->name, s->name);
        }
    }
    for (int i = 0; i < c->permits.count; i++) {
        JClass *p = c->permits.items[i];
        jt_complete(p);
        int direct = p->super && p->super->cls == c;
        for (int j = 0; j < p->ifaces.count; j++) if (((JType *)p->ifaces.items[j])->cls == c) direct = 1;
        if (!direct) cerr(c->decl, "'%s' is permitted by sealed %s but does not extend or implement it", p->name, c->name);
    }
    if ((c->flags & M_SEALED) && !c->permits.count && c->subclasses.count == 0) {
        /* no permits and no known subtypes in the loaded closure: allowed, but nothing to switch over */
    }
}

/* ---------------------------------------------------------------- bodies */

static void init_ctx(Ctx *cx, JClass *c, JMethod *m, int is_static) {
    memset(cx, 0, sizeof(*cx));
    cx->cls = c;
    cx->method = m;
    cx->is_static = is_static;
    cx->ret = TVOID;
}

static void finish_ctx(Ctx *cx) {
    for (int i = 0; i < cx->captures.count; i++) {
        JLocal *l = cx->captures.items[i];
        int allowed = l->has_init || l->is_param ? 0 : 1;
        if (l->assigns > allowed && l->captured_at) {
            cerr(l->captured_at, "local variable '%s' is captured by a lambda and must be effectively final", l->name);
            l->captured_at = NULL;
        }
    }
}

void check_method_body(JClass *c, JMethod *m) {
    Node *d = m->decl;
    if (!d || !d->b) return;
    Ctx cx;
    init_ctx(&cx, c, m, (m->flags & M_STATIC) != 0);
    cx.in_ctor = m->is_ctor;
    cx.ret = m->ret;
    ExFrame fr = {0};
    fr.kind = F_METHOD;
    for (int i = 0; i < m->throws.count; i++) j_list_push(&fr.types, m->throws.items[i]);
    cx.ex = &fr;
    for (int i = 0; i < m->params.count; i++) {
        JParam *p = m->params.items[i];
        Node *pn = (d->l1.count > i) ? d->l1.items[i] : d;
        JLocal *l = jt_declare_local(&cx, pn, p->name, p->type, (d->l1.count > i) ? pn->flags : 0, 1);
        l->has_init = 1;
    }
    Node *body = d->b;
    int explicit_call = body->l1.count > 0 && ((Node *)body->l1.items[0])->kind == N_CTORCALL;
    for (int i = 1; i < body->l1.count; i++) {
        Node *s = body->l1.items[i];
        if (s->kind == N_CTORCALL) cerr(s, "'%s(...)' must be the first statement of a constructor", s->name);
    }
    if (m->is_ctor && !explicit_call && c->kind == N_CLASS && c->super && !(m->flags & 0x20000)) {
        Ctx pre = cx;
        JMethod *chosen = NULL;
        JList none = {0};
        check_args_for_ctor(&pre, d, c->super->cls, c->super, &none, d, &chosen);
    }
    check_block_body(&cx, body);
    if (m->ret->kind != TY_VOID && !m->is_ctor && can_complete(body)) cerr(d, "missing return statement in '%s'", m->name);
    finish_ctx(&cx);
}

static void check_field_init(JClass *c, JField *f) {
    Node *d = f->decl;
    if (!d || d->kind != N_FIELD || !d->b) return;
    Ctx cx;
    init_ctx(&cx, c, NULL, (f->flags & M_STATIC) != 0);
    ExFrame fr = {0};
    fr.kind = F_INIT;
    cx.ex = &fr;
    check_coerce(&cx, d->b, f->type, "field initializer");
    finish_ctx(&cx);
}

static void check_enum_consts(JClass *c) {
    Node *d = c->decl;
    for (int i = 0; i < d->l4.count; i++) {
        Node *ec = d->l4.items[i];
        Ctx cx;
        init_ctx(&cx, c, NULL, 1);
        ExFrame fr = {0};
        fr.kind = F_INIT;
        cx.ex = &fr;
        for (int j = 0; j < ec->l1.count; j++) {
            Node *a = ec->l1.items[j];
            if (!is_poly_arg(a)) check_expr(&cx, a, NULL);
        }
        JMethod *k = NULL;
        check_args_for_ctor(&cx, ec, c, c->self_type, &ec->l1, ec, &k);
        ec->sym = k;
        finish_ctx(&cx);
    }
}

void jt_check_class(JClass *c) {
    if (c->checked) return;
    c->checked = 1;
    jt_complete(c);
    if (c->kind == N_RECORD) {
        for (int i = 0; i < c->fields.count; i++) {
            JField *f = c->fields.items[i];
            if (!(f->flags & M_FINAL)) cerr(f->decl, "record components are final");
        }
    }
    check_sealed(c);
    check_abstract_impl(c);
    check_overrides(c);
    if (c->kind != N_INTERFACE && !(c->flags & M_ABSTRACT)) {
        for (int i = 0; i < c->methods.count; i++) {
            JMethod *m = c->methods.items[i];
            if ((m->flags & M_ABSTRACT) && m->decl) cerr(m->decl, "class '%s' declares abstract method '%s' and must be declared abstract", c->name, m->name);
        }
    }
    if (c->kind == N_ENUM) check_enum_consts(c);
    for (int i = 0; i < c->fields.count; i++) check_field_init(c, c->fields.items[i]);
    for (int i = 0; i < c->decl->l1.count; i++) {
        Node *mem = c->decl->l1.items[i];
        if (mem->kind != N_INIT) continue;
        Ctx cx;
        init_ctx(&cx, c, NULL, (mem->flags & M_STATIC) != 0);
        cx.in_ctor = !(mem->flags & M_STATIC);
        ExFrame fr = {0};
        fr.kind = F_INIT;
        cx.ex = &fr;
        check_block_body(&cx, mem->b);
        finish_ctx(&cx);
    }
    for (int i = 0; i < c->methods.count; i++) check_method_body(c, c->methods.items[i]);
    for (int i = 0; i < c->ctors.count; i++) check_method_body(c, c->ctors.items[i]);
}

void jt_check_all(void) {
    for (int i = 0;; i++) {
        JClass **all;
        int n = jt_all_classes(&all);
        if (i >= n) break;
        jt_check_class(all[i]);
    }
}
