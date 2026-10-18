#include <stdlib.h>

#include "jparse/H/jparse.h"
#include "jparse/H/jparse_internal.h"

static Node *unary(Parser *p);
static Node *ternary(Parser *p);

static void args(Parser *p, JList *out) {
    p_expect(p, JT_LPAREN, "'('");
    if (!p_is(p, JT_RPAREN)) {
        do {
            j_list_push(out, p_assign_expr(p));
        } while (p_accept(p, JT_COMMA));
    }
    p_expect(p, JT_RPAREN, "')'");
}

/* Index of the token after the ')' matching the '(' at offset `from`, or -1. */
static int after_matching_paren(Parser *p, int from) {
    int depth = 0;
    for (int i = from;; i++) {
        JTokKind k = p_peek_at(p, i)->kind;
        if (k == JT_EOF) return -1;
        if (k == JT_LPAREN) depth++;
        if (k == JT_RPAREN && --depth == 0) return i + 1;
    }
}

static Node *lambda(Parser *p) {
    Node *l = p_node(p, N_LAMBDA);
    if (p_is(p, JT_IDENT)) {
        Node *pa = p_node(p, N_PARAM);
        pa->name = p_ident(p, NULL);
        j_list_push(&l->l1, pa);
    } else {
        p_expect(p, JT_LPAREN, "'('");
        if (!p_is(p, JT_RPAREN)) {
            do {
                Node *pa = p_node(p, N_PARAM);
                if (p_is(p, JT_FINAL)) p_next(p);
                if (p_is(p, JT_IDENT) && (p_peek_at(p, 1)->kind == JT_COMMA || p_peek_at(p, 1)->kind == JT_RPAREN)) {
                    pa->name = p_ident(p, NULL);
                } else {
                    pa->a = p_type(p);
                    pa->name = p_ident(p, "a parameter name");
                }
                j_list_push(&l->l1, pa);
            } while (p_accept(p, JT_COMMA));
        }
        p_expect(p, JT_RPAREN, "')'");
    }
    p_expect(p, JT_ARROW, "'->'");
    if (p_is(p, JT_LBRACE)) l->b = p_block(p);
    else l->b = p_assign_expr(p);
    return l;
}

static int lambda_ahead(Parser *p) {
    if (p->no_lambda) return 0;
    if (p_is(p, JT_IDENT) && p_peek_at(p, 1)->kind == JT_ARROW) return 1;
    if (p_is(p, JT_LPAREN)) {
        int after = after_matching_paren(p, 0);
        return after > 0 && p_peek_at(p, after)->kind == JT_ARROW;
    }
    return 0;
}

static Node *array_init(Parser *p, Node *elem_type, int dims) {
    Node *na = p_node(p, N_NEWARRAY);
    na->a = elem_type;
    na->flags = dims;
    Node *init = p_node(p, N_ARRAYINIT);
    p_expect(p, JT_LBRACE, "'{'");
    while (!p_is(p, JT_RBRACE)) {
        if (p_is(p, JT_LBRACE)) j_list_push(&init->l1, array_init(p, elem_type, dims - 1));
        else j_list_push(&init->l1, p_assign_expr(p));
        if (!p_accept(p, JT_COMMA)) break;
    }
    p_expect(p, JT_RBRACE, "'}'");
    na->b = init;
    return na;
}

static Node *new_expr(Parser *p) {
    Node *start = p_node(p, N_NEW);
    p_expect(p, JT_NEW, "'new'");
    Node *t = p_type(p);
    if (p_is(p, JT_LPAREN)) {
        start->a = t;
        args(p, &start->l1);
        if (p_is(p, JT_LBRACE)) p_fail(p, "anonymous classes are not supported; declare a named class or use a lambda");
        return start;
    }
    Node *na = p_node(p, N_NEWARRAY);
    na->line = start->line;
    na->col = start->col;
    na->a = t;
    int dims = t->flags;
    t->flags = 0;
    while (p_is(p, JT_LBRACKET)) {
        p_next(p);
        if (p_is(p, JT_RBRACKET)) {
            p_next(p);
        } else {
            j_list_push(&na->l1, p_expr(p));
            p_expect(p, JT_RBRACKET, "']'");
        }
        dims++;
    }
    if (dims == 0) p_fail(p, "expected '(' or '[' after 'new %s'", t->name);
    na->flags = dims;
    if (p_is(p, JT_LBRACE)) {
        Node *ai = array_init(p, t, dims);
        na->b = ai->b;
    } else if (na->l1.count == 0) {
        p_fail(p, "array creation needs a dimension or an initializer");
    }
    return na;
}

static int starts_unary_not_plus_minus(JTokKind k) {
    switch (k) {
        case JT_IDENT: case JT_INT_LIT: case JT_LONG_LIT: case JT_FLOAT_LIT: case JT_DOUBLE_LIT:
        case JT_CHAR_LIT: case JT_STRING_LIT: case JT_LPAREN: case JT_BANG: case JT_TILDE:
        case JT_NEW: case JT_THIS: case JT_SUPER: case JT_TRUE: case JT_FALSE: case JT_NULL:
        case JT_SWITCH:
            return 1;
        default:
            return p_is_prim(k);
    }
}

/* At '(': decide whether this is a cast; returns the type node (consuming `(Type)`) or NULL. */
static Node *try_cast(Parser *p) {
    int save = p->pos;
    p_next(p);
    Node *t = p_type_opt(p);
    if (t && p_is(p, JT_RPAREN)) {
        JTokKind after = p_peek_at(p, 1)->kind;
        int prim = p_is_prim(((JToken *)&p->t.toks[save + 1])->kind) && t->flags == 0;
        if (prim ? (starts_unary_not_plus_minus(after) || after == JT_PLUS || after == JT_MINUS ||
                    after == JT_INC || after == JT_DEC)
                 : starts_unary_not_plus_minus(after)) {
            p_next(p);
            return t;
        }
    }
    p->pos = save;
    return NULL;
}

static Node *primary(Parser *p) {
    JToken *t = p_peek(p);
    Node *n;
    switch (t->kind) {
        case JT_INT_LIT: n = p_node(p, N_LIT_INT); n->name = t->text; p_next(p); return n;
        case JT_LONG_LIT: n = p_node(p, N_LIT_LONG); n->name = t->text; p_next(p); return n;
        case JT_FLOAT_LIT: n = p_node(p, N_LIT_FLOAT); n->name = t->text; p_next(p); return n;
        case JT_DOUBLE_LIT: n = p_node(p, N_LIT_DOUBLE); n->name = t->text; p_next(p); return n;
        case JT_CHAR_LIT: n = p_node(p, N_LIT_CHAR); n->name = t->text; p_next(p); return n;
        case JT_STRING_LIT: n = p_node(p, N_LIT_STRING); n->name = t->text; p_next(p); return n;
        case JT_TRUE: n = p_node(p, N_LIT_BOOL); n->name = "true"; p_next(p); return n;
        case JT_FALSE: n = p_node(p, N_LIT_BOOL); n->name = "false"; p_next(p); return n;
        case JT_NULL: n = p_node(p, N_LIT_NULL); n->name = "null"; p_next(p); return n;
        case JT_THIS:
            n = p_node(p, N_THIS);
            p_next(p);
            return n;
        case JT_SUPER:
            n = p_node(p, N_SUPER);
            p_next(p);
            return n;
        case JT_NEW:
            return new_expr(p);
        case JT_SWITCH:
            return p_switch(p, 1);
        case JT_IDENT:
            if (lambda_ahead(p)) return lambda(p);
            if (p_peek_at(p, 1)->kind == JT_LPAREN) {
                n = p_node(p, N_CALL);
                n->name = t->text;
                p_next(p);
                args(p, &n->l1);
                return n;
            }
            n = p_node(p, N_NAME);
            n->name = t->text;
            p_next(p);
            return n;
        case JT_LPAREN: {
            if (lambda_ahead(p)) return lambda(p);
            p_next(p);
            n = p_expr(p);
            p_expect(p, JT_RPAREN, "')'");
            return n;
        }
        default:
            if (p_is_prim(t->kind) && p_peek_at(p, 1)->kind == JT_COLONCOLON) {
                n = p_node(p, N_NAME);
                n->name = t->text;
                p_next(p);
                return n;
            }
            p_fail(p, "expected an expression but found %s",
                   t->kind == JT_EOF ? "end of file" : j_fmt("'%s'", t->text));
    }
    return NULL;
}

static Node *postfix(Parser *p) {
    Node *e = primary(p);
    for (;;) {
        if (p_is(p, JT_DOT)) {
            p_next(p);
            if (p_is(p, JT_LT)) p_fail(p, "explicit generic method type arguments are not supported");
            Node *n;
            char *name = p_ident(p, "a member name");
            if (p_is(p, JT_LPAREN)) {
                n = p_node(p, N_CALL);
                n->a = e;
                n->name = name;
                if (e->kind == N_SUPER) n->flags = 1;
                args(p, &n->l1);
            } else {
                n = p_node(p, N_FIELDACC);
                n->a = e;
                n->name = name;
            }
            n->line = e->line;
            n->col = e->col;
            e = n;
        } else if (p_is(p, JT_LBRACKET)) {
            Node *n = p_node(p, N_INDEX);
            p_next(p);
            n->a = e;
            n->b = p_expr(p);
            n->line = e->line;
            n->col = e->col;
            p_expect(p, JT_RBRACKET, "']'");
            e = n;
        } else if (p_is(p, JT_INC) || p_is(p, JT_DEC)) {
            Node *n = p_node(p, N_POSTFIX);
            n->name = p_peek(p)->text;
            p_next(p);
            n->a = e;
            n->line = e->line;
            n->col = e->col;
            e = n;
        } else if (p_is(p, JT_COLONCOLON)) {
            Node *n = p_node(p, N_METHODREF);
            p_next(p);
            n->a = e;
            n->name = p_is(p, JT_NEW) ? "new" : NULL;
            if (n->name) p_next(p);
            else n->name = p_ident(p, "a method name");
            n->line = e->line;
            n->col = e->col;
            e = n;
        } else {
            return e;
        }
    }
}

static Node *unary(Parser *p) {
    JToken *t = p_peek(p);
    if (t->kind == JT_PLUS || t->kind == JT_MINUS || t->kind == JT_BANG || t->kind == JT_TILDE ||
        t->kind == JT_INC || t->kind == JT_DEC) {
        Node *n = p_node(p, N_UNARY);
        n->name = t->text;
        p_next(p);
        n->a = unary(p);
        return n;
    }
    if (t->kind == JT_AMP) {
        /* `&x` shared borrow, `&mut x` exclusive borrow: only meaningful as a call argument (checked by jown) */
        Node *n = p_node(p, N_UNARY);
        n->name = "&";
        p_next(p);
        JToken *m = p_peek(p);
        if (m->kind == JT_IDENT && !strcmp(m->text, "mut") && (p_peek_at(p, 1)->kind == JT_IDENT || p_peek_at(p, 1)->kind == JT_THIS)) {
            n->name = "&mut";
            p_next(p);
        }
        n->a = unary(p);
        return n;
    }
    if (t->kind == JT_LPAREN && !lambda_ahead(p)) {
        Node *line_node = p_node(p, N_CAST);
        Node *type = try_cast(p);
        if (type) {
            line_node->a = type;
            line_node->b = unary(p);
            return line_node;
        }
    }
    return postfix(p);
}

/* Reads a binary operator at the cursor. Returns precedence (0 = none) and sets the
 * operator text and how many tokens it spans. `>` runs are merged into shifts here. */
static int peek_binop(Parser *p, const char **text, int *ntok) {
    JToken *t = p_peek(p);
    *ntok = 1;
    switch (t->kind) {
        case JT_OROR: *text = "||"; return 1;
        case JT_ANDAND: *text = "&&"; return 2;
        case JT_PIPE: *text = "|"; return 3;
        case JT_CARET: *text = "^"; return 4;
        case JT_AMP: *text = "&"; return 5;
        case JT_EQ: *text = "=="; return 6;
        case JT_NE: *text = "!="; return 6;
        case JT_LT: *text = "<"; return 7;
        case JT_LE: *text = "<="; return 7;
        case JT_GE: *text = ">="; return 7;
        case JT_INSTANCEOF: *text = "instanceof"; return 7;
        case JT_SHL: *text = "<<"; return 8;
        case JT_PLUS: *text = "+"; return 9;
        case JT_MINUS: *text = "-"; return 9;
        case JT_STAR: *text = "*"; return 10;
        case JT_SLASH: *text = "/"; return 10;
        case JT_PERCENT: *text = "%"; return 10;
        case JT_GT: {
            JToken *n1 = p_peek_at(p, 1);
            if (n1->kind == JT_GE && n1->adj) return 0; /* >>= */
            if (n1->kind == JT_GT && n1->adj) {
                JToken *n2 = p_peek_at(p, 2);
                if (n2->kind == JT_GE && n2->adj) return 0; /* >>>= */
                if (n2->kind == JT_GT && n2->adj) {
                    *text = ">>>";
                    *ntok = 3;
                    return 8;
                }
                *text = ">>";
                *ntok = 2;
                return 8;
            }
            *text = ">";
            return 7;
        }
        default:
            return 0;
    }
}

static Node *binary(Parser *p, int min_prec) {
    Node *lhs = unary(p);
    for (;;) {
        const char *op;
        int ntok;
        int prec = peek_binop(p, &op, &ntok);
        if (prec == 0 || prec < min_prec) return lhs;
        Node *n = p_node(p, N_BINARY);
        n->name = op;
        n->line = lhs->line;
        n->col = lhs->col;
        for (int i = 0; i < ntok; i++) p_next(p);
        if (strcmp(op, "instanceof") == 0) {
            n->kind = N_INSTANCEOF;
            n->a = lhs;
            if (p_is(p, JT_FINAL)) p_next(p);
            p->no_nullable++;
            n->b = p_type(p);
            p->no_nullable--;
            n->name = NULL;
            if (p_is(p, JT_IDENT)) n->name = p_ident(p, NULL);
            lhs = n;
            continue;
        }
        n->a = lhs;
        n->b = binary(p, prec + 1);
        lhs = n;
    }
}

static Node *ternary(Parser *p) {
    Node *c = binary(p, 1);
    if (!p_is(p, JT_QUESTION)) return c;
    Node *n = p_node(p, N_COND);
    n->line = c->line;
    n->col = c->col;
    p_next(p);
    n->a = c;
    n->b = p_assign_expr(p);
    p_expect(p, JT_COLON, "':'");
    n->c = p_assign_expr(p);
    return n;
}

static const char *assign_op(Parser *p, int *ntok) {
    *ntok = 1;
    switch (p_peek(p)->kind) {
        case JT_ASSIGN: return "=";
        case JT_PLUS_ASSIGN: return "+=";
        case JT_MINUS_ASSIGN: return "-=";
        case JT_STAR_ASSIGN: return "*=";
        case JT_SLASH_ASSIGN: return "/=";
        case JT_PERCENT_ASSIGN: return "%=";
        case JT_AMP_ASSIGN: return "&=";
        case JT_PIPE_ASSIGN: return "|=";
        case JT_CARET_ASSIGN: return "^=";
        case JT_SHL_ASSIGN: return "<<=";
        case JT_GT: {
            JToken *n1 = p_peek_at(p, 1);
            if (n1->kind == JT_GE && n1->adj) {
                *ntok = 2;
                return ">>=";
            }
            if (n1->kind == JT_GT && n1->adj) {
                JToken *n2 = p_peek_at(p, 2);
                if (n2->kind == JT_GE && n2->adj) {
                    *ntok = 3;
                    return ">>>=";
                }
            }
            return NULL;
        }
        default:
            return NULL;
    }
}

Node *p_assign_expr(Parser *p) {
    if (lambda_ahead(p)) return lambda(p);
    Node *lhs = ternary(p);
    int ntok;
    const char *op = assign_op(p, &ntok);
    if (!op) return lhs;
    Node *n = p_node(p, N_ASSIGN);
    n->line = lhs->line;
    n->col = lhs->col;
    n->name = op;
    for (int i = 0; i < ntok; i++) p_next(p);
    n->a = lhs;
    n->b = p_assign_expr(p);
    return n;
}

Node *p_expr(Parser *p) { return p_assign_expr(p); }
