#include <string.h>

#include "jparse/H/jparse_internal.h"

/* Old-Ash syntax is diagnosed by context, not by reserving the words (decision D3): `say`,
 * `next`, `each`, `no`... stay ordinary identifiers in new Ash, so a message is only produced
 * where the old construct is unambiguous. Each message names the replacement. */

static int continues_expr(JTokKind k) {
    switch (k) {
        case JT_ASSIGN: case JT_DOT: case JT_SEMI: case JT_LBRACKET: case JT_INC: case JT_DEC: case JT_COMMA:
        case JT_COLON: case JT_PLUS_ASSIGN: case JT_MINUS_ASSIGN: case JT_STAR_ASSIGN: case JT_SLASH_ASSIGN:
        case JT_PERCENT_ASSIGN: case JT_AMP_ASSIGN: case JT_PIPE_ASSIGN: case JT_CARET_ASSIGN: case JT_SHL_ASSIGN:
        case JT_RPAREN: case JT_RBRACE:
            return 1;
        default:
            return 0;
    }
}

static int starts_value(JTokKind k) {
    switch (k) {
        case JT_IDENT: case JT_INT_LIT: case JT_LONG_LIT: case JT_FLOAT_LIT: case JT_DOUBLE_LIT: case JT_CHAR_LIT:
        case JT_STRING_LIT: case JT_NEW: case JT_TRUE: case JT_FALSE: case JT_NULL: case JT_THIS: case JT_BANG:
            return 1;
        default:
            return 0;
    }
}

/* index of the token after the parenthesis group that starts at offset `from`, or -1 */
static int after_group(Parser *p, int from) {
    int depth = 0;
    for (int i = from;; i++) {
        JTokKind k = p_peek_at(p, i)->kind;
        if (k == JT_EOF) return -1;
        if (k == JT_LPAREN) depth++;
        if (k == JT_RPAREN && --depth == 0) return i + 1;
    }
}

static void old(Parser *p, const char *word, const char *use) {
    p_fail(p, "'%s' is old Ash syntax and was removed; %s (see MIGRATION.md)", word, use);
}

void p_old_syntax(Parser *p, int where) {
    JToken *t = p_peek(p);
    JTokKind n = p_peek_at(p, 1)->kind;
    if (t->kind == JT_AT && n == JT_IMPORT) {
        old(p, "@import <file.ash>;", "write `import package.Type;` at the top of the file, after the package declaration");
    }
    if (t->kind != JT_IDENT) return;
    const char *w = t->text;
    if (where == 2) {
        if (!strcmp(w, "local") || !strcmp(w, "say") || !strcmp(w, "forge") || !strcmp(w, "given") || !strcmp(w, "during") ||
            !strcmp(w, "each") || !strcmp(w, "attempt") || !strcmp(w, "raise")) {
            p_fail(p, "this looks like an old Ash script ('%s' at the top level); new Ash programs declare a package and a class "
                      "with `public static void main(String[] args)` (see MIGRATION.md)", w);
        }
        return;
    }
    if (where == 1) {
        if (!strcmp(w, "forge") && n == JT_IDENT) old(p, "forge", "declare a method with explicit types, e.g. `static int name(int a) { ... }`");
        if (!strcmp(w, "local") && n == JT_IDENT) old(p, "local", "declare the field with its type, e.g. `private int x;`");
        return;
    }
    /* statement position */
    if (!strcmp(w, "local") && n == JT_IDENT) old(p, "local", "declare the variable with its type: `int x = 5;` or `var x = 5;`");
    if (!strcmp(w, "say") && starts_value(n)) old(p, "say", "use `System.out.println(value);`");
    if ((!strcmp(w, "given") || !strcmp(w, "during")) && !continues_expr(n)) {
        int ok = n != JT_LPAREN || (after_group(p, 1) > 0 && p_peek_at(p, after_group(p, 1))->kind == JT_LBRACE);
        if (ok) {
            if (!strcmp(w, "given")) old(p, "given", "use `if (condition) { ... }`");
            old(p, "during", "use `while (condition) { ... }`");
        }
    }
    if (!strcmp(w, "otherwise") && (n == JT_LBRACE || (n == JT_IDENT && !strcmp(p_peek_at(p, 1)->text, "given")))) {
        old(p, "otherwise", "use `else { ... }` or `else if (...)`");
    }
    if (!strcmp(w, "each") && n == JT_IDENT && p_peek_at(p, 2)->kind == JT_IDENT && !strcmp(p_peek_at(p, 2)->text, "in")) {
        old(p, "each x in xs", "use `for (Type x : xs) { ... }`");
    }
    if (!strcmp(w, "stop") && n == JT_SEMI) old(p, "stop", "use `break;`");
    if (!strcmp(w, "next") && n == JT_SEMI) old(p, "next", "use `continue;`");
    if (!strcmp(w, "yield") && !continues_expr(n)) old(p, "yield", "use `return value;`");
    if (!strcmp(w, "attempt") && n == JT_LBRACE) old(p, "attempt", "use `try { ... } catch (Exception e) { ... }`");
    if (!strcmp(w, "raise") && starts_value(n)) old(p, "raise", "use `throw new SomeException(...);`");
    if (!strcmp(w, "forge") && n == JT_IDENT) old(p, "forge", "declare a method with explicit types, or write a lambda `(a) -> expr`");
}
