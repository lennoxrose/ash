#include <stdlib.h>

#include "jparse/H/jparse.h"
#include "jparse/H/jparse_internal.h"

Node *p_block(Parser *p) {
    Node *b = p_node(p, N_BLOCK);
    p_expect(p, JT_LBRACE, "'{'");
    while (!p_is(p, JT_RBRACE)) {
        if (p_is(p, JT_EOF)) p_fail(p, "unexpected end of file: missing '}'");
        j_list_push(&b->l1, p_stmt(p));
    }
    p_expect(p, JT_RBRACE, "'}'");
    return b;
}

/* `shared` is a modifier only when a type follows (`shared Label l = ...`); elsewhere it is an ordinary name */
static int p_is_shared_modifier(Parser *p) {
    if (!p_is_word(p, "shared")) return 0;
    JTokKind k = p_peek_at(p, 1)->kind;
    return k == JT_IDENT || (p_is_prim(k) && k != JT_VOID);
}

int p_looks_like_local_decl(Parser *p) {
    JToken *t = p_peek(p);
    if (t->kind == JT_FINAL) return 1;
    if (p_is_shared_modifier(p)) return 1;
    if (p_is_prim(t->kind) && t->kind != JT_VOID) {
        return p_peek_at(p, 1)->kind != JT_COLONCOLON && p_peek_at(p, 1)->kind != JT_DOT;
    }
    if (t->kind != JT_IDENT) return 0;
    if (strcmp(t->text, "var") == 0 && p_peek_at(p, 1)->kind == JT_IDENT) return 1;
    int save = p->pos;
    Node *ty = p_type_opt(p);
    int ok = 0;
    if (ty && p_is(p, JT_IDENT)) {
        JTokKind k = p_peek_at(p, 1)->kind;
        ok = (k == JT_ASSIGN || k == JT_SEMI || k == JT_COMMA || k == JT_COLON || k == JT_RPAREN);
    }
    p->pos = save;
    return ok;
}

Node *p_array_init(Parser *p, Node *type) {
    Node *na = p_node(p, N_NEWARRAY);
    na->a = type;
    na->flags = type->flags;
    Node *init = p_node(p, N_ARRAYINIT);
    p_expect(p, JT_LBRACE, "'{'");
    while (!p_is(p, JT_RBRACE)) {
        if (p_is(p, JT_LBRACE)) {
            Node *inner = p_array_init(p, type);
            inner->flags = na->flags - 1;
            j_list_push(&init->l1, inner);
        } else {
            j_list_push(&init->l1, p_assign_expr(p));
        }
        if (!p_accept(p, JT_COMMA)) break;
    }
    p_expect(p, JT_RBRACE, "'}'");
    na->b = init;
    return na;
}

Node *p_local_decl(Parser *p, int final_flag, int require_semi) {
    int flags = final_flag ? M_FINAL : 0;
    while (p_is(p, JT_FINAL) || p_is_shared_modifier(p)) {
        flags |= p_is(p, JT_FINAL) ? M_FINAL : M_SHARED;
        p_next(p);
    }
    Node *type = NULL;
    if (p_is_word(p, "var") && p_peek_at(p, 1)->kind == JT_IDENT) {
        p_next(p);
    } else {
        type = p_type(p);
    }
    Node *first = NULL, *seq = NULL;
    for (;;) {
        Node *l = p_node(p, N_LOCAL);
        l->flags = flags;
        l->a = type;
        l->name = p_ident(p, "a variable name");
        if (p_accept(p, JT_ASSIGN)) {
            if (p_is(p, JT_LBRACE)) {
                if (!type) p_fail(p, "cannot infer the type of an array initializer with 'var'");
                l->b = p_array_init(p, type);
            } else {
                l->b = p_assign_expr(p);
            }
        }
        if (!first) first = l;
        else {
            if (!seq) {
                seq = p_node(p, N_SEQ);
                j_list_push(&seq->l1, first);
            }
            j_list_push(&seq->l1, l);
        }
        if (!p_accept(p, JT_COMMA)) break;
    }
    if (require_semi) p_expect(p, JT_SEMI, "';'");
    return seq ? seq : first;
}

static Node *paren_cond(Parser *p) {
    p_expect(p, JT_LPAREN, "'('");
    Node *c = p_expr(p);
    p_expect(p, JT_RPAREN, "')'");
    return c;
}

static Node *for_stmt(Parser *p) {
    Node *f = p_node(p, N_FOR);
    p_next(p);
    p_expect(p, JT_LPAREN, "'('");
    if (!p_is(p, JT_SEMI)) {
        if (p_looks_like_local_decl(p)) {
            /* foreach? `Type name :` */
            int save = p->pos;
            int is_each = 0;
            while (p_is(p, JT_FINAL)) p_next(p);
            Node *ty = NULL;
            if (p_is_word(p, "var") && p_peek_at(p, 1)->kind == JT_IDENT) p_next(p);
            else ty = p_type_opt(p);
            if ((ty || p->pos > save) && p_is(p, JT_IDENT) && p_peek_at(p, 1)->kind == JT_COLON) is_each = 1;
            if (is_each) {
                Node *e = p_node(p, N_FOREACH);
                e->line = f->line;
                e->col = f->col;
                e->a = ty;
                e->name = p_ident(p, NULL);
                p_expect(p, JT_COLON, "':'");
                e->b = p_expr(p);
                p_expect(p, JT_RPAREN, "')'");
                e->c = p_stmt(p);
                return e;
            }
            p->pos = save;
            j_list_push(&f->l1, p_local_decl(p, 0, 0));
        } else {
            do {
                Node *es = p_node(p, N_EXPRSTMT);
                es->a = p_expr(p);
                j_list_push(&f->l1, es);
            } while (p_accept(p, JT_COMMA));
        }
    }
    p_expect(p, JT_SEMI, "';'");
    if (!p_is(p, JT_SEMI)) f->a = p_expr(p);
    p_expect(p, JT_SEMI, "';'");
    if (!p_is(p, JT_RPAREN)) {
        do {
            j_list_push(&f->l2, p_expr(p));
        } while (p_accept(p, JT_COMMA));
    }
    p_expect(p, JT_RPAREN, "')'");
    f->b = p_stmt(p);
    return f;
}

static Node *try_stmt(Parser *p) {
    Node *t = p_node(p, N_TRY);
    p_next(p);
    if (p_is(p, JT_LPAREN)) p_fail(p, "try-with-resources is not supported");
    t->b = p_block(p);
    while (p_is(p, JT_CATCH)) {
        Node *c = p_node(p, N_CATCH);
        p_next(p);
        p_expect(p, JT_LPAREN, "'('");
        while (p_accept(p, JT_FINAL)) {}
        do {
            j_list_push(&c->l1, p_type(p));
        } while (p_accept(p, JT_PIPE));
        c->name = p_ident(p, "an exception variable name");
        p_expect(p, JT_RPAREN, "')'");
        c->b = p_block(p);
        j_list_push(&t->l1, c);
    }
    if (p_accept(p, JT_FINALLY)) t->c = p_block(p);
    if (t->l1.count == 0 && !t->c) p_fail(p, "'try' needs at least one 'catch' or a 'finally'");
    return t;
}

static Node *case_label(Parser *p) {
    /* type pattern: `Type name` followed by `->` or `:` */
    int save = p->pos;
    if (p_is(p, JT_IDENT)) {
        Node *ty = p_type_opt(p);
        if (ty && p_is(p, JT_IDENT) &&
            (p_peek_at(p, 1)->kind == JT_ARROW || p_peek_at(p, 1)->kind == JT_COLON ||
             (p_peek_at(p, 1)->kind == JT_IDENT && !strcmp(p_peek_at(p, 1)->text, "when")))) {
            Node *tp = p_node(p, N_TYPEPATTERN);
            tp->a = ty;
            tp->name = p_ident(p, NULL);
            if (p_is(p, JT_IDENT) && !strcmp(p_peek(p)->text, "when")) { /* `case Integer i when i > 3 ->` */
                p_next(p);
                p->no_lambda++;
                tp->b = p_assign_expr(p);                 /* the guard; its `->` is the arm's, not a lambda's */
                p->no_lambda--;
            }
            return tp;
        }
    }
    p->pos = save;
    p->no_lambda++;
    Node *e = p_assign_expr(p);
    p->no_lambda--;
    return e;
}

Node *p_switch(Parser *p, int is_expr) {
    Node *sw = p_node(p, N_SWITCH);
    sw->flags = is_expr;
    p_next(p);
    sw->a = paren_cond(p);
    p_expect(p, JT_LBRACE, "'{'");
    Node *pending = NULL; /* stacked `case A: case B:` labels merge into one case */
    while (!p_is(p, JT_RBRACE)) {
        if (p_is(p, JT_EOF)) p_fail(p, "unexpected end of file in switch");
        Node *c = pending ? pending : p_node(p, N_CASE);
        pending = NULL;
        if (p_accept(p, JT_DEFAULT)) {
            c->flags = 1;
        } else {
            p_expect(p, JT_CASE, "'case' or 'default'");
            do {
                if (p_is(p, JT_NULL)) { /* case null */
                    j_list_push(&c->l1, p_assign_expr(p));
                } else {
                    j_list_push(&c->l1, case_label(p));
                }
            } while (p_accept(p, JT_COMMA));
        }
        if (p_accept(p, JT_ARROW)) {
            c->str = "->";
            Node *body;
            if (p_is(p, JT_LBRACE)) {
                body = p_block(p);
            } else if (p_is(p, JT_THROW)) {
                body = p_stmt(p);
            } else {
                body = p_node(p, N_EXPRSTMT);
                body->a = p_expr(p);
                p_expect(p, JT_SEMI, "';'");
            }
            c->b = body;
        } else {
            p_expect(p, JT_COLON, "':' or '->'");
            while (!p_is(p, JT_CASE) && !p_is(p, JT_DEFAULT) && !p_is(p, JT_RBRACE)) {
                if (p_is(p, JT_EOF)) p_fail(p, "unexpected end of file in switch");
                j_list_push(&c->l2, p_stmt(p));
            }
            if (c->l2.count == 0 && (p_is(p, JT_CASE) || p_is(p, JT_DEFAULT))) {
                if (p_is(p, JT_DEFAULT)) { /* `case A: default:` */
                    c->flags = c->flags;
                }
                pending = c;
                continue;
            }
        }
        j_list_push(&sw->l1, c);
    }
    if (pending) j_list_push(&sw->l1, pending);
    p_expect(p, JT_RBRACE, "'}'");
    return sw;
}

Node *p_stmt(Parser *p) {
    p_old_syntax(p, 0);
    JToken *t = p_peek(p);
    Node *n;
    switch (t->kind) {
        case JT_LBRACE: return p_block(p);
        case JT_SEMI:
            n = p_node(p, N_EMPTY);
            p_next(p);
            return n;
        case JT_IF:
            n = p_node(p, N_IF);
            p_next(p);
            n->a = paren_cond(p);
            n->b = p_stmt(p);
            if (p_accept(p, JT_ELSE)) n->c = p_stmt(p);
            return n;
        case JT_WHILE:
            n = p_node(p, N_WHILE);
            p_next(p);
            n->a = paren_cond(p);
            n->b = p_stmt(p);
            return n;
        case JT_DO:
            n = p_node(p, N_DO);
            p_next(p);
            n->b = p_stmt(p);
            p_expect(p, JT_WHILE, "'while'");
            n->a = paren_cond(p);
            p_expect(p, JT_SEMI, "';'");
            return n;
        case JT_FOR: return for_stmt(p);
        case JT_RETURN:
            n = p_node(p, N_RETURN);
            p_next(p);
            if (!p_is(p, JT_SEMI)) n->a = p_expr(p);
            p_expect(p, JT_SEMI, "';'");
            return n;
        case JT_BREAK:
            n = p_node(p, N_BREAK);
            p_next(p);
            if (p_is(p, JT_IDENT)) n->name = p_ident(p, NULL);    /* `break outer;` */
            p_expect(p, JT_SEMI, "';'");
            return n;
        case JT_CONTINUE:
            n = p_node(p, N_CONTINUE);
            p_next(p);
            if (p_is(p, JT_IDENT)) n->name = p_ident(p, NULL);    /* `continue outer;` */
            p_expect(p, JT_SEMI, "';'");
            return n;
        case JT_IDENT:
            if (p_peek_at(p, 1)->kind == JT_COLON) {              /* `outer: for (...) { ... }` */
                const char *label = t->text;
                p_next(p);
                p_next(p);
                int k = p_peek(p)->kind;
                if (k != JT_FOR && k != JT_WHILE && k != JT_DO) p_fail(p, "a label can only precede a for, while or do loop");
                n = p_stmt(p);
                n->str = label;
                return n;
            }
            break;
        case JT_THROW:
            n = p_node(p, N_THROW);
            p_next(p);
            n->a = p_expr(p);
            p_expect(p, JT_SEMI, "';'");
            return n;
        case JT_TRY: return try_stmt(p);
        case JT_SWITCH: {
            Node *sw = p_switch(p, 0);
            return sw;
        }
        case JT_THIS:
        case JT_SUPER:
            if (p_peek_at(p, 1)->kind == JT_LPAREN) {
                n = p_node(p, N_CTORCALL);
                n->name = t->text;
                p_next(p);
                p_expect(p, JT_LPAREN, "'('");
                if (!p_is(p, JT_RPAREN)) {
                    do {
                        j_list_push(&n->l1, p_assign_expr(p));
                    } while (p_accept(p, JT_COMMA));
                }
                p_expect(p, JT_RPAREN, "')'");
                p_expect(p, JT_SEMI, "';'");
                return n;
            }
            break;
        default:
            break;
    }
    if (t->kind == JT_CLASS || t->kind == JT_INTERFACE || t->kind == JT_ENUM) {
        p_fail(p, "local classes are not supported; declare the type at the top level or as a member");
    }
    if (p_looks_like_local_decl(p)) return p_local_decl(p, 0, 1);
    n = p_node(p, N_EXPRSTMT);
    n->a = p_expr(p);
    p_expect(p, JT_SEMI, "';'");
    return n;
}
