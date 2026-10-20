#include <ctype.h>
#include <string.h>

#include "jcommon/H/jcommon.h"
#include "jtypes/H/check.h"
#include "jtypes/H/jtypes.h"

/* "did you mean": the name in `cands` closest to `name`, or NULL. Used only on the error path (a name that
 * does not resolve), so nothing here runs for a program that checks clean. */

/* optimal string alignment distance (insert, delete, substitute, swap two neighbours), case-sensitive */
static int distance(const char *a, const char *b) {
    int la = (int)strlen(a), lb = (int)strlen(b);
    if (la > 63 || lb > 63) return 99;
    int d[64][64];
    for (int i = 0; i <= la; i++) d[i][0] = i;
    for (int j = 0; j <= lb; j++) d[0][j] = j;
    for (int i = 1; i <= la; i++) {
        for (int j = 1; j <= lb; j++) {
            int cost = a[i - 1] != b[j - 1];
            int v = d[i - 1][j] + 1;
            if (d[i][j - 1] + 1 < v) v = d[i][j - 1] + 1;
            if (d[i - 1][j - 1] + cost < v) v = d[i - 1][j - 1] + cost;
            if (i > 1 && j > 1 && a[i - 1] == b[j - 2] && a[i - 2] == b[j - 1] && d[i - 2][j - 2] + 1 < v) v = d[i - 2][j - 2] + 1;
            d[i][j] = v;
        }
    }
    return d[la][lb];
}

static int same_ignoring_case(const char *a, const char *b) {
    for (; *a && *b; a++, b++) if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
    return !*a && !*b;
}

const char *sug_best(const char *name, JList *cands) {
    int len = (int)strlen(name);
    const char *best = NULL;
    int best_d = 99;
    for (int i = 0; i < cands->count; i++) {
        const char *c = cands->items[i];
        if (!strcmp(c, name)) continue;
        int d;
        if (same_ignoring_case(c, name)) d = 0;                    /* `Count` for `count` beats any edit */
        else if (len < 2 || (int)strlen(c) < 2) continue;           /* one-letter names are all "close" to each other */
        else {
            d = distance(name, c);
            if (d > (len > 3 ? len : 3) / 3) continue;              /* one edit up to 5 letters, two up to 8, ... */
        }
        if (d < best_d) { best = c; best_d = d; }
    }
    return best;
}

static void add_name(JList *out, const char *n) {
    if (!n || !*n || n[0] == '<' || (n[0] == '_' && n[1] == '_')) return;
    for (int i = 0; i < out->count; i++) if (!strcmp(out->items[i], n)) return;
    j_list_push(out, (void *)n);
}

void sug_locals(Ctx *cx, JList *out) {
    for (int i = cx->locals.count - 1; i >= 0; i--) {
        JLocal *l = cx->locals.items[i];
        if (!l->is_temp) add_name(out, l->name);
    }
}

/* the fields (want_methods == 0) or methods of `c` and everything it inherits that the caller can use */
static void walk_members(Ctx *cx, JClass *c, int want_methods, int static_only, JList *out, int depth) {
    if (!c || depth > 16) return;
    jt_complete(c);
    if (want_methods) {
        for (int i = 0; i < c->methods.count; i++) {
            JMethod *m = c->methods.items[i];
            if ((static_only && !(m->flags & M_STATIC)) || !accessible(cx, m->cls, m->flags)) continue;
            add_name(out, m->name);
        }
    } else {
        for (int i = 0; i < c->fields.count; i++) {
            JField *f = c->fields.items[i];
            if ((static_only && !(f->flags & M_STATIC)) || !accessible(cx, f->cls, f->flags)) continue;
            add_name(out, f->name);
        }
    }
    if (c->super) walk_members(cx, c->super->cls, want_methods, static_only, out, depth + 1);
    for (int i = 0; i < c->ifaces.count; i++) walk_members(cx, ((JType *)c->ifaces.items[i])->cls, want_methods, static_only, out, depth + 1);
}

void sug_members(Ctx *cx, JClass *c, int want_methods, int static_only, JList *out) {
    walk_members(cx, c, want_methods, static_only, out, 0);
    if (want_methods && c && c->kind == N_INTERFACE) walk_members(cx, jt_object(), 1, static_only, out, 0);   /* interfaces answer to Object's methods too */
}

/* a name that has to be a value: locals, then the fields of the enclosing classes */
const char *sug_value_name(Ctx *cx, const char *name) {
    JList cands = {0};
    sug_locals(cx, &cands);
    for (JClass *c = cx->cls; c; c = c->outer) sug_members(cx, c, 0, cx->is_static, &cands);
    return sug_best(name, &cands);
}

/* "help: did you mean ..." for a name that does not resolve; call before the cerr that reports it */
void sug_help(const char *what, const char *guess, const char *suffix) {
    if (guess) j_diag_help("did you mean the %s '%s'%s?", what, guess, suffix);
}
