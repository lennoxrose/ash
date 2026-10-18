#ifndef JPARSE_INTERNAL_H
#define JPARSE_INTERNAL_H

#include <string.h>
#include "jlex/H/jlex.h"
#include "jparse/H/jast.h"

typedef struct {
    JTokens t;
    int pos;
    int no_nullable; /* set while parsing instanceof types (`x instanceof T ? a : b`) */
    int no_lambda; /* set while parsing case labels, where `X ->` is not a lambda */
} Parser;

static inline JToken *p_peek(Parser *p) { return &p->t.toks[p->pos]; }
static inline JToken *p_peek_at(Parser *p, int n) {
    int i = p->pos + n;
    if (i >= p->t.count) i = p->t.count - 1;
    return &p->t.toks[i];
}
static inline int p_is(Parser *p, JTokKind k) { return p_peek(p)->kind == k; }
static inline int p_is_word(Parser *p, const char *w) {
    return p_peek(p)->kind == JT_IDENT && strcmp(p_peek(p)->text, w) == 0;
}

JToken *p_next(Parser *p);
int p_accept(Parser *p, JTokKind k);
JToken *p_expect(Parser *p, JTokKind k, const char *what);
void p_fail(Parser *p, const char *fmt, ...) __attribute__((noreturn));
Node *p_node(Parser *p, NodeKind k);
char *p_ident(Parser *p, const char *what);

/* types (jparse_decl.c) */
Node *p_type_opt(Parser *p);          /* NULL when the tokens are not a type (no error) */
Node *p_type(Parser *p);
int p_is_prim(JTokKind k);

/* declarations (jparse_decl.c) */
Node *p_class_body_decl(Parser *p, Node *owner);
int p_parse_modifiers(Parser *p, JList *annos);
Node *p_type_decl(Parser *p, int mods, JList *annos);
void p_params(Parser *p, Node *method);

/* old-syntax migration diagnostics (jparse_old.c); where: 0 statement, 1 class member, 2 top level */
void p_old_syntax(Parser *p, int where);

/* statements and expressions (jparse_stmt.c, jparse_expr.c) */
Node *p_block(Parser *p);
Node *p_array_init(Parser *p, Node *type); /* `{a, b}` initializer for an array of `type` */
Node *p_stmt(Parser *p);
Node *p_expr(Parser *p);
Node *p_assign_expr(Parser *p);
Node *p_switch(Parser *p, int is_expr);
Node *p_local_decl(Parser *p, int final_flag, int require_semi);
int p_looks_like_local_decl(Parser *p);

#endif
