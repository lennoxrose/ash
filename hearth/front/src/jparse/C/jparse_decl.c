#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "jparse/H/jparse.h"
#include "jparse/H/jparse_internal.h"

Node *node_new(NodeKind kind, const char *path, int line, int col) {
    Node *n = j_alloc(sizeof(Node));
    n->kind = kind;
    n->path = path;
    n->line = line;
    n->col = col;
    return n;
}

Node *p_node(Parser *p, NodeKind k) {
    JToken *t = p_peek(p);
    return node_new(k, p->t.path, t->line, t->col);
}

void p_fail(Parser *p, const char *fmt, ...) {
    JToken *t = p_peek(p);
    va_list ap;
    va_start(ap, fmt);
    char *msg = NULL;
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    msg = j_alloc((size_t)n + 1);
    va_start(ap, fmt);
    vsnprintf(msg, (size_t)n + 1, fmt, ap);
    va_end(ap);
    j_error(p->t.path, t->line, t->col, "%s", msg);
    abort();
}

JToken *p_next(Parser *p) {
    JToken *t = p_peek(p);
    if (t->kind != JT_EOF) p->pos++;
    return t;
}

int p_accept(Parser *p, JTokKind k) {
    if (p_is(p, k)) {
        p->pos++;
        return 1;
    }
    return 0;
}

JToken *p_expect(Parser *p, JTokKind k, const char *what) {
    if (!p_is(p, k)) {
        JToken *t = p_peek(p);
        p_fail(p, "expected %s but found %s%s%s", what ? what : jtok_name(k),
               t->kind == JT_EOF ? "end of file" : "'", t->kind == JT_EOF ? "" : t->text,
               t->kind == JT_EOF ? "" : "'");
    }
    return p_next(p);
}

char *p_ident(Parser *p, const char *what) {
    JToken *t = p_expect(p, JT_IDENT, what ? what : "an identifier");
    return (char *)t->text;
}

int p_is_prim(JTokKind k) {
    return k == JT_INT || k == JT_LONG || k == JT_FLOAT || k == JT_DOUBLE || k == JT_BOOL ||
           k == JT_CHAR || k == JT_BYTE || k == JT_SHORT || k == JT_VOID;
}

/* ---------------------------------------------------------------- types */

static Node *type_args_opt(Parser *p, Node *t);

static Node *type_arg(Parser *p) {
    if (p_is(p, JT_QUESTION)) {
        Node *w = p_node(p, N_WILDCARD);
        p_next(p);
        w->name = "";
        if (p_is(p, JT_EXTENDS) || p_is(p, JT_SUPER)) {
            w->name = p_peek(p)->kind == JT_EXTENDS ? "extends" : "super";
            p_next(p);
            w->a = p_type_opt(p);
            if (!w->a) return NULL;
        }
        return w;
    }
    return p_type_opt(p);
}

static Node *type_args_opt(Parser *p, Node *t) {
    if (!p_is(p, JT_LT)) return t;
    p_next(p);
    if (p_is(p, JT_GT)) { /* diamond */
        p_next(p);
        t->flags |= 0x10000;
        return t;
    }
    for (;;) {
        Node *a = type_arg(p);
        if (!a) return NULL;
        j_list_push(&t->l1, a);
        if (p_accept(p, JT_COMMA)) continue;
        if (p_accept(p, JT_GT)) break;
        return NULL;
    }
    return t;
}

Node *p_type_opt(Parser *p) {
    Node *t = p_node(p, N_TYPE);
    JToken *tok = p_peek(p);
    if (p_is_prim(tok->kind)) {
        t->name = tok->text;
        p_next(p);
    } else if (tok->kind == JT_IDENT) {
        JBuf name = {0};
        j_buf_add(&name, tok->text);
        p_next(p);
        t = type_args_opt(p, t);
        if (!t) return NULL;
        while (p_is(p, JT_DOT) && p_peek_at(p, 1)->kind == JT_IDENT) {
            p_next(p);
            j_buf_addc(&name, '.');
            j_buf_add(&name, p_peek(p)->text);
            p_next(p);
            t = type_args_opt(p, t);
            if (!t) return NULL;
        }
        t->name = name.data;
    } else {
        return NULL;
    }
    while (p_is(p, JT_LBRACKET) && p_peek_at(p, 1)->kind == JT_RBRACKET) {
        p_next(p);
        p_next(p);
        t->flags++;
    }
    /* `T?` nullable: only when what follows cannot continue an expression like `a ? b : c`
     * in a way that makes the `?` a ternary; callers backtrack when the parse fails. */
    if (p_is(p, JT_QUESTION) && !p->no_nullable) {
        JTokKind k = p_peek_at(p, 1)->kind;
        if (k == JT_IDENT || k == JT_RPAREN || k == JT_COMMA || k == JT_GT || k == JT_LBRACKET ||
            k == JT_ELLIPSIS || k == JT_SEMI || k == JT_ASSIGN || k == JT_LBRACE || k == JT_COLON) {
            p_next(p);
            t->nullable = 1;
        }
    }
    return t;
}

Node *p_type(Parser *p) {
    Node *t = p_type_opt(p);
    if (!t) p_fail(p, "expected a type");
    return t;
}

/* ---------------------------------------------------------------- modifiers */

static Node *annotation(Parser *p) {
    Node *a = p_node(p, N_ANNO);
    p_expect(p, JT_AT, "'@'");
    a->name = p_ident(p, "an annotation name");
    if (p_accept(p, JT_LPAREN)) {
        if (p_is(p, JT_STRING_LIT)) {
            a->str = p_peek(p)->text;
            p_next(p);
        } else {
            while (!p_is(p, JT_RPAREN) && !p_is(p, JT_EOF)) p_next(p);
        }
        p_expect(p, JT_RPAREN, "')'");
    }
    return a;
}

int p_parse_modifiers(Parser *p, JList *annos) {
    int mods = 0;
    for (;;) {
        JToken *t = p_peek(p);
        int m = 0;
        switch (t->kind) {
            case JT_PUBLIC: m = M_PUBLIC; break;
            case JT_PRIVATE: m = M_PRIVATE; break;
            case JT_PROTECTED: m = M_PROTECTED; break;
            case JT_STATIC: m = M_STATIC; break;
            case JT_FINAL: m = M_FINAL; break;
            case JT_ABSTRACT: m = M_ABSTRACT; break;
            case JT_NATIVE: m = M_NATIVE; break;
            case JT_CONST: m = M_CONST | M_STATIC | M_FINAL; break;
            case JT_DEFAULT:
                /* `default` here is the interface-method modifier, not a switch label */
                if (p_peek_at(p, 1)->kind == JT_COLON || p_peek_at(p, 1)->kind == JT_ARROW) return mods;
                m = M_DEFAULT;
                break;
            case JT_AT:
                if (p_peek_at(p, 1)->kind == JT_INTERFACE) return mods;
                j_list_push(annos, annotation(p));
                continue;
            case JT_IDENT:
                if (strcmp(t->text, "sealed") == 0 &&
                    (p_peek_at(p, 1)->kind == JT_CLASS || p_peek_at(p, 1)->kind == JT_INTERFACE ||
                     p_peek_at(p, 1)->kind == JT_ABSTRACT || p_peek_at(p, 1)->kind == JT_PUBLIC)) {
                    m = M_SEALED;
                    break;
                }
                return mods;
            default:
                return mods;
        }
        if (mods & m) p_fail(p, "repeated modifier '%s'", t->text);
        mods |= m;
        p_next(p);
    }
}

/* ---------------------------------------------------------------- declarations */

static void type_params_opt(Parser *p, JList *out) {
    if (!p_accept(p, JT_LT)) return;
    for (;;) {
        Node *tp = p_node(p, N_TYPEPARAM);
        tp->name = p_ident(p, "a type parameter name");
        if (p_accept(p, JT_EXTENDS)) {
            tp->a = p_type(p);
            while (p_accept(p, JT_AMP)) p_type(p); /* extra bounds are checked loosely */
        }
        j_list_push(out, tp);
        if (p_accept(p, JT_COMMA)) continue;
        p_expect(p, JT_GT, "'>'");
        break;
    }
}

static Node *param(Parser *p) {
    Node *pa = p_node(p, N_PARAM);
    while (p_is(p, JT_FINAL) || p_is(p, JT_AT)) {
        if (p_accept(p, JT_FINAL)) pa->flags |= M_FINAL;
        else {
            JList dummy = {0};
            j_list_push(&dummy, annotation(p));
        }
    }
    pa->a = p_type(p);
    if (p_accept(p, JT_ELLIPSIS)) {
        pa->flags |= M_VARARGS;
        pa->a->flags++;
    }
    pa->name = p_ident(p, "a parameter name");
    return pa;
}

void p_params(Parser *p, Node *m) {
    p_expect(p, JT_LPAREN, "'('");
    if (!p_is(p, JT_RPAREN)) {
        do {
            j_list_push(&m->l1, param(p));
        } while (p_accept(p, JT_COMMA));
    }
    p_expect(p, JT_RPAREN, "')'");
}

static void throws_opt(Parser *p, Node *m) {
    if (!p_accept(p, JT_THROWS)) return;
    do {
        j_list_push(&m->l3, p_type(p));
    } while (p_accept(p, JT_COMMA));
}

static void type_list(Parser *p, JList *out) {
    do {
        j_list_push(out, p_type(p));
    } while (p_accept(p, JT_COMMA));
}

static Node *enum_const(Parser *p) {
    Node *c = p_node(p, N_ENUMCONST);
    while (p_is(p, JT_AT)) annotation(p);
    c->name = p_ident(p, "an enum constant name");
    if (p_accept(p, JT_LPAREN)) {
        if (!p_is(p, JT_RPAREN)) {
            do {
                j_list_push(&c->l1, p_assign_expr(p));
            } while (p_accept(p, JT_COMMA));
        }
        p_expect(p, JT_RPAREN, "')'");
    }
    return c;
}

static void class_body(Parser *p, Node *cls) {
    p_expect(p, JT_LBRACE, "'{'");
    if (cls->kind == N_ENUM) {
        while (p_is(p, JT_IDENT) || p_is(p, JT_AT)) {
            j_list_push(&cls->l4, enum_const(p));
            if (!p_accept(p, JT_COMMA)) break;
        }
        if (!p_accept(p, JT_SEMI) && !p_is(p, JT_RBRACE)) p_fail(p, "expected ';' or '}' after enum constants");
    }
    while (!p_is(p, JT_RBRACE)) {
        if (p_is(p, JT_EOF)) p_fail(p, "unexpected end of file in class body");
        if (p_accept(p, JT_SEMI)) continue;
        j_list_push(&cls->l1, p_class_body_decl(p, cls));
    }
    p_expect(p, JT_RBRACE, "'}'");
}

Node *p_type_decl(Parser *p, int mods, JList *annos) {
    JToken *kw = p_peek(p);
    NodeKind kind;
    if (kw->kind == JT_CLASS) kind = N_CLASS;
    else if (kw->kind == JT_INTERFACE) kind = N_INTERFACE;
    else if (kw->kind == JT_ENUM) kind = N_ENUM;
    else if (kw->kind == JT_IDENT && strcmp(kw->text, "record") == 0) kind = N_RECORD;
    else p_fail(p, "expected class, interface, enum or record declaration");
    Node *c = p_node(p, kind);
    p_next(p);
    c->flags = mods;
    c->l5 = (JList){0};
    c->name = p_ident(p, "a type name");
    (void)annos;
    type_params_opt(p, &c->l2);
    if (kind == N_RECORD) {
        p_expect(p, JT_LPAREN, "'('");
        if (!p_is(p, JT_RPAREN)) {
            do {
                j_list_push(&c->l4, param(p));
            } while (p_accept(p, JT_COMMA));
        }
        p_expect(p, JT_RPAREN, "')'");
    }
    if (p_accept(p, JT_EXTENDS)) {
        if (kind == N_INTERFACE) type_list(p, &c->l3);
        else c->a = p_type(p);
    }
    if (p_accept(p, JT_IMPLEMENTS)) type_list(p, &c->l3);
    if (p_is_word(p, "permits")) {
        p_next(p);
        type_list(p, &c->l5);
    }
    class_body(p, c);
    return c;
}

Node *p_class_body_decl(Parser *p, Node *owner) {
    p_old_syntax(p, 1);
    JList annos = {0};
    int mods = p_parse_modifiers(p, &annos);
    JToken *t = p_peek(p);

    if (t->kind == JT_LBRACE && (mods & ~M_STATIC) == 0) { /* initializer block */
        Node *in = p_node(p, N_INIT);
        in->flags = mods;
        in->b = p_block(p);
        return in;
    }
    if (t->kind == JT_CLASS || t->kind == JT_INTERFACE || t->kind == JT_ENUM ||
        (t->kind == JT_IDENT && strcmp(t->text, "record") == 0 && p_peek_at(p, 1)->kind == JT_IDENT &&
         (p_peek_at(p, 2)->kind == JT_LPAREN || p_peek_at(p, 2)->kind == JT_LT))) {
        return p_type_decl(p, mods, &annos);
    }

    Node *m = p_node(p, N_METHOD);
    m->flags = mods;
    m->l4 = annos;
    if (p_is(p, JT_LT)) type_params_opt(p, &m->l2);

    /* constructor: Name '(' (or compact record constructor: Name '{') */
    if (p_is(p, JT_IDENT) && strcmp(p_peek(p)->text, owner->name) == 0 &&
        (p_peek_at(p, 1)->kind == JT_LPAREN ||
         (owner->kind == N_RECORD && p_peek_at(p, 1)->kind == JT_LBRACE))) {
        m->kind = N_CTOR;
        m->name = p_peek(p)->text;
        p_next(p);
        if (p_is(p, JT_LPAREN)) {
            p_params(p, m);
        } else {
            m->flags |= 0x20000; /* compact canonical constructor */
        }
        throws_opt(p, m);
        m->b = p_block(p);
        return m;
    }

    Node *type = p_type(p);
    m->a = type;
    m->name = p_ident(p, "a member name");
    if (p_is(p, JT_LPAREN)) {
        p_params(p, m);
        /* legacy array dims after params are not supported */
        throws_opt(p, m);
        if (p_is(p, JT_LBRACE)) {
            m->b = p_block(p);
        } else {
            p_expect(p, JT_SEMI, "'{' or ';'");
        }
        return m;
    }
    /* field(s) */
    Node *first = NULL;
    Node *seq = NULL;
    for (;;) {
        Node *f = (first == NULL) ? m : node_new(N_FIELD, p->t.path, t->line, t->col);
        f->kind = N_FIELD;
        f->flags = mods;
        f->l1 = annos;
        f->a = type;
        if (first) f->name = p_ident(p, "a field name");
        if (p_accept(p, JT_ASSIGN)) {
            if (p_is(p, JT_LBRACE)) {
                f->b = p_array_init(p, type);
            } else {
                f->b = p_assign_expr(p);
            }
        }
        if (!first) {
            first = f;
        } else {
            if (!seq) {
                seq = p_node(p, N_SEQ);
                j_list_push(&seq->l1, first);
            }
            j_list_push(&seq->l1, f);
        }
        if (p_accept(p, JT_COMMA)) continue;
        break;
    }
    p_expect(p, JT_SEMI, "';'");
    return seq ? seq : first;
}

/* ---------------------------------------------------------------- compilation unit */

static char *dotted_name(Parser *p, int *wildcard) {
    JBuf b = {0};
    j_buf_add(&b, p_ident(p, "a name"));
    *wildcard = 0;
    while (p_accept(p, JT_DOT)) {
        if (p_accept(p, JT_STAR)) {
            *wildcard = 1;
            break;
        }
        j_buf_addc(&b, '.');
        j_buf_add(&b, p_ident(p, "a name"));
    }
    return b.data;
}

const char *jparse_capture_path;
JTokens jparse_captured;

Node *jparse_unit(const char *path, const char *src) {
    j_register_source(path, src);
    Parser p = {jlex_tokenize(path, src), 0, 0, 0};
    if (jparse_capture_path && strcmp(path, jparse_capture_path) == 0) jparse_captured = p.t;
    Node *u = p_node(&p, N_UNIT);
    u->name = "";
    p_old_syntax(&p, 2);
    if (p_is(&p, JT_PACKAGE)) {
        p_next(&p);
        int w;
        u->name = dotted_name(&p, &w);
        p_expect(&p, JT_SEMI, "';'");
    }
    while (p_is(&p, JT_IMPORT)) {
        Node *im = p_node(&p, N_IMPORT);
        p_next(&p);
        if (p_is(&p, JT_STATIC)) p_fail(&p, "static imports are not supported");
        int w;
        im->name = dotted_name(&p, &w);
        im->flags = w;
        if (p_is_word(&p, "as")) p_fail(&p, "import aliases are not supported (decision D1)");
        p_expect(&p, JT_SEMI, "';'");
        j_list_push(&u->l1, im);
    }
    while (!p_is(&p, JT_EOF)) {
        if (p_accept(&p, JT_SEMI)) continue;
        p_old_syntax(&p, 2);
        JList annos = {0};
        int mods = p_parse_modifiers(&p, &annos);
        j_list_push(&u->l2, p_type_decl(&p, mods, &annos));
    }
    return u;
}
