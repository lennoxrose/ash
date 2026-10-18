#include <string.h>

#include "jlower/H/jlower_internal.h"

/* Class lowering. Object model (old-Ash values only):
 *   object      map {"__c": qname, "__vt": C__VT, "__is": C__IS, <fields>...}
 *   statics     module-level map  C__S    (also holds enum constants and "$VALUES")
 *   methods     top-level forge   C__name_<id>(self, args...)
 *   dispatch    __vcN(obj, "name/arity", args...) through the flat vtable C__VT
 *   subtyping   C__IS maps every supertype qname to yes (instanceof, catch)
 * Interfaces and "native value" classes (String, boxes, List, Map...) have no instances of
 * their own: their methods are @Lower templates inlined at the call site. */

static void vt_collect(JClass *c, JList *keys, JList *meths) {
    jt_complete(c);
    if (c->super) vt_collect(c->super->cls, keys, meths);
    /* interface default methods only fill gaps; superclass methods win */
    JList ifs = {0};
    for (int i = 0; i < c->ifaces.count; i++) j_list_push(&ifs, ((JType *)c->ifaces.items[i])->cls);
    for (int q = 0; q < ifs.count; q++) {
        JClass *ic = ifs.items[q];
        jt_complete(ic);
        for (int i = 0; i < ic->methods.count; i++) {
            JMethod *m = ic->methods.items[i];
            if ((m->flags & (M_ABSTRACT | M_STATIC)) || !m->decl || !m->decl->b) continue;
            char *k = method_key(m);
            int found = 0;
            for (int j = 0; j < keys->count; j++) if (!strcmp(keys->items[j], k)) found = 1;
            if (!found) { j_list_push(keys, k); j_list_push(meths, m); }
        }
        for (int i = 0; i < ic->ifaces.count; i++) j_list_push(&ifs, ((JType *)ic->ifaces.items[i])->cls);
    }
    for (int i = 0; i < c->methods.count; i++) {
        JMethod *m = c->methods.items[i];
        if ((m->flags & (M_ABSTRACT | M_STATIC)) || m->tmpl) continue;
        if (!(m->decl && m->decl->b) && !m->synthetic) continue;
        if (m->synthetic && !(m->is_record_accessor || (c->kind == N_RECORD && m->synthetic))) continue;
        char *k = method_key(m);
        int idx = -1;
        for (int j = 0; j < keys->count; j++) if (!strcmp(keys->items[j], k)) idx = j;
        if (idx >= 0) meths->items[idx] = m;
        else { j_list_push(keys, k); j_list_push(meths, m); }
    }
}

static int is_lowered(JClass *c) { return !is_native_value_class(c); }

static void add_is(JClass *c, JList *seen) {
    for (int i = 0; i < seen->count; i++) if (seen->items[i] == c) return;
    j_list_push(seen, c);
    jt_complete(c);
    if (c->super) add_is(c->super->cls, seen);
    for (int i = 0; i < c->ifaces.count; i++) add_is(((JType *)c->ifaces.items[i])->cls, seen);
}

static char *default_for(JType *t) {
    if (ty_is_prim(t)) return t->kind == TY_BOOL ? "no" : "0";
    return "none";
}

static void collect_inst_fields(JClass *c, JBuf *b) {
    if (c->super) collect_inst_fields(c->super->cls, b);
    for (int i = 0; i < c->fields.count; i++) {
        JField *f = c->fields.items[i];
        if ((f->flags & M_STATIC) || f->tmpl) continue;
        j_buf_addf(b, ", %s: %s", quote_str(f->name), default_for(f->type));
    }
}

/* ---------------------------------------------------------------- bodies */

static void begin_body(JClass *c, JMethod *m, int is_static) {
    L.cls = c;
    L.method = m;
    L.is_static = is_static;
    L.loop = NULL;
    L.fin_n = 0;
    L.pre.len = 0;
}

static char *param_list(JMethod *m, int with_self) {
    JBuf b = {0};
    int any = 0;
    if (with_self) { j_buf_add(&b, "self"); any = 1; }
    Node *d = m->decl;
    for (int i = 0; i < m->params.count; i++) {
        if (any) j_buf_add(&b, ", ");
        const char *nm = ((JParam *)m->params.items[i])->name;
        (void)d;
        j_buf_add(&b, sname(nm));
        any = 1;
    }
    return b.data ? b.data : (char *)"";
}

static void emit_method(JBuf *out, JClass *c, JMethod *m) {
    begin_body(c, m, (m->flags & M_STATIC) != 0);
    emit_line(out, 0, "forge %s(%s) {", fn_name(m, NULL), param_list(m, !(m->flags & M_STATIC)));
    lblock_body(out, m->decl->b, 1);
    emit_line(out, 1, "yield none;");
    emit_line(out, 0, "}");
}

static void emit_record_members(JBuf *out, JClass *c) {
    for (int i = 0; i < c->methods.count; i++) {
        JMethod *m = c->methods.items[i];
        if (!m->synthetic) continue;
        if (m->is_record_accessor) {
            emit_line(out, 0, "forge %s(self) { yield self[%s]; }", fn_name(m, NULL), quote_str(m->name));
        } else if (!strcmp(m->name, "toString")) {
            JBuf b = {0};
            j_buf_addf(&b, "%s", quote_str(j_fmt("%s[", c->name)));
            for (int k = 0; k < c->fields.count; k++) {
                JField *f = c->fields.items[k];
                if (f->flags & M_STATIC) continue;
                j_buf_addf(&b, " + %s + %s", quote_str(j_fmt("%s%s=", k ? ", " : "", f->name)),
                           str_of(f->type, j_fmt("self[%s]", quote_str(f->name))));
            }
            j_buf_add(&b, " + \"]\"");
            emit_line(out, 0, "forge %s(self) { yield %s; }", fn_name(m, NULL), b.data);
        } else if (!strcmp(m->name, "equals")) {
            JBuf b = {0};
            j_buf_add(&b, "yes");
            for (int k = 0; k < c->fields.count; k++) {
                JField *f = c->fields.items[k];
                if (f->flags & M_STATIC) continue;
                j_buf_addf(&b, " and __eq(self[%s], o[%s])", quote_str(f->name), quote_str(f->name));
            }
            emit_line(out, 0, "forge %s(self, o) { given not __inst(o, %s) { yield no; } yield %s; }", fn_name(m, NULL),
                      quote_str(c->qname), b.data);
        } else if (!strcmp(m->name, "hashCode")) {
            JBuf b = {0};
            j_buf_add(&b, "0");
            for (int k = 0; k < c->fields.count; k++) {
                JField *f = c->fields.items[k];
                if (f->flags & M_STATIC) continue;
                j_buf_addf(&b, " * 31 + __hash(self[%s])", quote_str(f->name));
            }
            emit_line(out, 0, "forge %s(self) { yield (%s) %% 4294967296; }", fn_name(m, NULL), b.data);
        }
    }
}

static void emit_field_inits(JBuf *out, JClass *c) {
    for (int i = 0; i < c->decl->l1.count; i++) {
        Node *mem = c->decl->l1.items[i];
        JList one = {0};
        if (mem->kind == N_SEQ) one = mem->l1;
        else j_list_push(&one, mem);
        for (int k = 0; k < one.count; k++) {
            Node *f = one.items[k];
            if (f->kind == N_FIELD && !(f->flags & M_STATIC) && f->b && c->kind != N_INTERFACE) {
                char *v = lx(f->b);
                flush_pre(out, 1);
                emit_line(out, 1, "self[%s] = %s;", quote_str(f->name), v);
            } else if (f->kind == N_INIT && !(f->flags & M_STATIC)) {
                lblock_body(out, f->b, 1);
            }
        }
    }
}

static void emit_ctor(JBuf *out, JClass *c, JMethod *k) {
    begin_body(c, k, 0);
    Node *d = k->decl;
    int is_enum = c->kind == N_ENUM;
    emit_line(out, 0, "forge %s(%s) {", fn_name(k, "init"), param_list(k, 1));
    int first = 0;
    int delegates = 0;
    if (d && d->b && d->b->l1.count && ((Node *)d->b->l1.items[0])->kind == N_CTORCALL) {
        Node *cc = d->b->l1.items[0];
        delegates = !strcmp(cc->name, "this");
        ls(out, cc, 1);
        first = 1;
    } else if (c->super && c->kind == N_CLASS) {
        JMethod *sk = find_ctor(c->super->cls, 0);
        const char *q = c->super->cls->qname;
        if (sk && !sk->tmpl && strcmp(q, "ash.lang.Object") != 0) emit_line(out, 1, "%s(self);", fn_name(sk, "init"));
    }
    if (!delegates) emit_field_inits(out, c);
    if (d && d->b) {
        for (int i = first; i < d->b->l1.count; i++) ls(out, d->b->l1.items[i], 1);
    }
    if (c->kind == N_RECORD && (!d || (d->flags & 0x20000))) {
        for (int i = 0; i < c->decl->l4.count; i++) {
            const char *n = ((Node *)c->decl->l4.items[i])->name;
            emit_line(out, 1, "self[%s] = %s;", quote_str(n), sname(n));
        }
    }
    emit_line(out, 0, "}");
    if (c->flags & M_ABSTRACT) return;
    /* allocator */
    JBuf obj = {0};
    j_buf_addf(&obj, "{\"__c\": %s, \"__vt\": %s, \"__is\": %s", quote_str(c->qname), cls_vt(c), cls_is(c));
    collect_inst_fields(c, &obj);
    j_buf_addc(&obj, '}');
    char *plist = param_list(k, 0);
    if (is_enum) {
        emit_line(out, 0, "forge %s(__name, __ord%s%s) {", fn_name(k, "new"), *plist ? ", " : "", plist);
        emit_line(out, 1, "local self = %s;", obj.data);
        emit_line(out, 1, "self[\"__name\"] = __name;");
        emit_line(out, 1, "self[\"__ord\"] = __ord;");
    } else {
        emit_line(out, 0, "forge %s(%s) {", fn_name(k, "new"), plist);
        emit_line(out, 1, "local self = %s;", obj.data);
    }
    emit_line(out, 1, "%s(%s);", fn_name(k, "init"), *plist ? j_fmt("self, %s", plist) : "self");
    emit_line(out, 1, "yield self;");
    emit_line(out, 0, "}");
}

static void emit_clinit(JBuf *out, JClass *c) {
    begin_body(c, NULL, 1);
    emit_line(out, 0, "forge %s__clinit() {", c->mangled);
    if (c->kind == N_ENUM) {
        Node *d = c->decl;
        for (int i = 0; i < d->l4.count; i++) {
            Node *ec = d->l4.items[i];
            JMethod *k = ec->sym;
            if (!k) continue;
            JBuf args = {0};
            for (int j = 0; j < ec->l1.count; j++) {
                j_buf_add(&args, ", ");
                j_buf_add(&args, lx(ec->l1.items[j]));
            }
            flush_pre(out, 1);
            emit_line(out, 1, "%s[%s] = %s(%s, %d%s);", cls_state(c), quote_str(ec->name), fn_name(k, "new"),
                      quote_str(ec->name), i, args.data ? args.data : "");
            emit_line(out, 1, "push(%s[\"$VALUES\"], %s[%s]);", cls_state(c), cls_state(c), quote_str(ec->name));
        }
    }
    for (int i = 0; i < c->decl->l1.count; i++) {
        Node *mem = c->decl->l1.items[i];
        JList one = {0};
        if (mem->kind == N_SEQ) one = mem->l1;
        else j_list_push(&one, mem);
        for (int k = 0; k < one.count; k++) {
            Node *f = one.items[k];
            int stat = (f->flags & M_STATIC) || c->kind == N_INTERFACE;
            if (f->kind == N_FIELD && stat && f->b) {
                char *v = lx(f->b);
                flush_pre(out, 1);
                emit_line(out, 1, "%s[%s] = %s;", cls_state(c), quote_str(f->name), v);
            } else if (f->kind == N_INIT && (f->flags & M_STATIC)) {
                lblock_body(out, f->b, 1);
            }
        }
    }
    emit_line(out, 0, "}");
}

/* ---------------------------------------------------------------- ordering */

static void scan(Node *n, JList *deps) {
    if (!n) return;
    if (n->kind == N_NAME || n->kind == N_FIELDACC) {
        if (n->sym && (n->rk == R_STATIC_FIELD || n->rk == R_ENUM_CONST)) j_list_push(deps, ((JField *)n->sym)->cls);
    } else if ((n->kind == N_CALL || n->kind == N_NEW) && n->sym) {
        j_list_push(deps, ((JMethod *)n->sym)->cls);
    }
    scan(n->a, deps);
    scan(n->b, deps);
    scan(n->c, deps);
    JList *ls_[5] = {&n->l1, &n->l2, &n->l3, &n->l4, &n->l5};
    for (int k = 0; k < 5; k++) for (int i = 0; i < ls_[k]->count; i++) scan(ls_[k]->items[i], deps);
}

static void visit(JClass *c, JList *done, JList *active) {
    for (int i = 0; i < done->count; i++) if (done->items[i] == c) return;
    for (int i = 0; i < active->count; i++) if (active->items[i] == c) return;
    j_list_push(active, c);
    jt_complete(c);
    JList deps = {0};
    if (c->super) j_list_push(&deps, c->super->cls);
    for (int i = 0; i < c->ifaces.count; i++) j_list_push(&deps, ((JType *)c->ifaces.items[i])->cls);
    for (int i = 0; i < c->decl->l4.count && c->kind == N_ENUM; i++) scan(c->decl->l4.items[i], &deps);
    for (int i = 0; i < c->decl->l1.count; i++) {
        Node *mem = c->decl->l1.items[i];
        JList one = {0};
        if (mem->kind == N_SEQ) one = mem->l1;
        else j_list_push(&one, mem);
        for (int k = 0; k < one.count; k++) {
            Node *f = one.items[k];
            if ((f->kind == N_FIELD && ((f->flags & M_STATIC) || c->kind == N_INTERFACE)) || (f->kind == N_INIT && (f->flags & M_STATIC))) scan(f, &deps);
        }
    }
    for (int i = 0; i < deps.count; i++) {
        JClass *d = deps.items[i];
        if (d != c && is_lowered(d)) visit(d, done, active);
    }
    active->count--;
    j_list_push(done, c);
}

/* ---------------------------------------------------------------- program */

static JMethod *find_main(JClass *entry) {
    JClass **all;
    int n = jt_all_classes(&all);
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < n; i++) {
            JClass *c = all[i];
            if (pass == 0 && c != entry) continue;
            if (pass == 1 && strncmp(c->qname, "ash.", 4) == 0) continue;
            for (int j = 0; j < c->methods.count; j++) {
                JMethod *m = c->methods.items[j];
                if (strcmp(m->name, "main") || !(m->flags & M_STATIC) || m->ret->kind != TY_VOID || m->params.count != 1) continue;
                JType *p = ((JParam *)m->params.items[0])->type;
                if (p->kind == TY_ARRAY && ty_is_string(p->elem)) return m;
            }
        }
    }
    return NULL;
}

static void emit_class(JBuf *decls, JBuf *out, JClass *c) {
    emit_line(out, 0, "// ---- %s %s", c->kind == N_INTERFACE ? "interface" : c->kind == N_ENUM ? "enum" : c->kind == N_RECORD ? "record" : "class", c->qname);
    JBuf s = {0};
    j_buf_add(&s, "{\"$VALUES\": []");
    for (int i = 0; i < c->fields.count; i++) {
        JField *f = c->fields.items[i];
        if (!(f->flags & M_STATIC) || f->tmpl) continue;
        j_buf_addf(&s, ", %s: %s", quote_str(f->name), f->is_enum_const ? "none" : default_for(f->type));
    }
    j_buf_addc(&s, '}');
    emit_line(decls, 0, "local %s = %s;", cls_state(c), s.data);
    JList sup = {0};
    add_is(c, &sup);
    JBuf is = {0};
    j_buf_addc(&is, '{');
    for (int i = 0; i < sup.count; i++) j_buf_addf(&is, "%s%s: yes", i ? ", " : "", quote_str(((JClass *)sup.items[i])->qname));
    j_buf_addc(&is, '}');
    emit_line(decls, 0, "local %s = %s;", cls_is(c), is.data);
    if (c->kind != N_INTERFACE) emit_line(decls, 0, "local %s = {};", cls_vt(c));
    for (int i = 0; i < c->methods.count; i++) {
        JMethod *m = c->methods.items[i];
        if (m->tmpl || (m->flags & (M_ABSTRACT | M_NATIVE)) || !m->decl || !m->decl->b) continue;
        emit_method(out, c, m);
    }
    if (c->kind == N_RECORD) emit_record_members(out, c);
    if (c->kind != N_INTERFACE) {
        for (int i = 0; i < c->ctors.count; i++) {
            JMethod *k = c->ctors.items[i];
            if (!k->tmpl) emit_ctor(out, c, k);
        }
    }
    emit_clinit(out, c);
}

char *jlower_program(JClass *entry) {
    static const char *core[] = {
        "ash.lang.Object", "ash.lang.String", "ash.lang.Enum", "ash.lang.Record", "ash.lang.Throwable", "ash.lang.Exception",
        "ash.lang.RuntimeException", "ash.lang.ArithmeticException", "ash.lang.IndexOutOfBoundsException",
        "ash.lang.KeyNotFoundException", "ash.lang.NullPointerException", "ash.lang.ClassCastException",
        "ash.lang.ParseException", "ash.lang.IllegalArgumentException", "ash.lang.System", NULL
    };
    for (int i = 0; core[i]; i++) jt_complete(jt_well_known(core[i]));
    jt_check_all();
    if (j_error_count()) return NULL;
    JMethod *main_m = find_main(entry);
    if (!main_m) {
        cerr(entry ? entry->decl : NULL, "no entry point: declare 'public static void main(String[] args)'");
        return NULL;
    }
    JBuf out = {0}, decls = {0}, code = {0};
    JBuf prelude = {0};
    emit_prelude(&prelude);
    /* emit the entry class, then every class its code reaches (tree-shaking by use) */
    mark_used(main_m->cls);
    JList lowered = {0};
    for (int i = 0; i < used_count(); i++) {
        JClass *c = used_at(i);
        if (!is_lowered(c)) continue;
        j_list_push(&lowered, c);
        emit_class(&decls, &code, c);
        /* supertypes are needed for vtables and instanceof tables */
        if (c->super) mark_used(c->super->cls);
        for (int k = 0; k < c->ifaces.count; k++) mark_used(((JType *)c->ifaces.items[k])->cls);
    }
    /* module-level variables first: functions can only see variables declared above them */
    if (decls.data) j_buf_add(&out, decls.data);
    if (code.data) j_buf_add(&out, code.data);
    /* keep only the runtime helpers the program (transitively) calls: the old VM allows 256 functions */
    /* wiring */
    JList order = {0}, active = {0};
    for (int i = 0; i < lowered.count; i++) visit(lowered.items[i], &order, &active);
    emit_line(&out, 0, "forge start() {");
    for (int i = 0; i < lowered.count; i++) {
        JClass *c = lowered.items[i];
        if (c->kind == N_INTERFACE) continue;
        JList keys = {0}, meths = {0};
        vt_collect(c, &keys, &meths);
        for (int k = 0; k < keys.count; k++) {
            emit_line(&out, 1, "%s[%s] = %s;", cls_vt(c), quote_str(keys.items[k]), fn_name(meths.items[k], NULL));
        }
    }
    for (int i = 0; i < order.count; i++) emit_line(&out, 1, "%s__clinit();", ((JClass *)order.items[i])->mangled);
    emit_line(&out, 1, "attempt {");
    emit_line(&out, 2, "%s([]);", fn_name(main_m, NULL));
    emit_line(&out, 1, "} handle (__e) {");
    emit_line(&out, 2, "__uncaught(__e);");
    emit_line(&out, 1, "}");
    emit_line(&out, 0, "}");
    if (j_error_count()) return NULL;
    /* header + the module without anything unreachable from start() */
    JBuf final = {0};
    j_buf_add(&final, "// Internal IR generated by hearth/front. Not Ash source.\n");
    j_buf_add(&final, prelude.data ? prelude.data : "");
    j_buf_add(&final, out.data);
    return prune_module(final.data);
}
