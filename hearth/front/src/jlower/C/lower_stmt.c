#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "jlower/H/jlower_internal.h"
#include "jown/H/region.h"
#include "jown/H/own.h"

/* Statement lowering: control flow, switch (as if-chains), try/catch/finally via
 * attempt/handle, lambdas, and the immediately-invoked closure used by switch expressions. */

void emit_line(JBuf *b, int ind, const char *fmt, ...) {
    for (int i = 0; i < ind; i++) j_buf_add(b, "    ");
    va_list ap;
    va_start(ap, fmt);
    char *s;
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    s = j_alloc((size_t)n + 1);
    va_start(ap, fmt);
    vsnprintf(s, (size_t)n + 1, fmt, ap);
    va_end(ap);
    j_buf_add(b, s);
    j_buf_addc(b, '\n');
}

void flush_pre(JBuf *b, int ind) {
    if (!L.pre.data || !L.pre.len) return;
    char *p = L.pre.data;
    while (*p) {
        char *nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);
        for (int i = 0; i < ind; i++) j_buf_add(b, "    ");
        j_buf_addn(b, p, len);
        j_buf_addc(b, '\n');
        if (!nl) break;
        p = nl + 1;
    }
    L.pre.len = 0;
    if (L.pre.data) L.pre.data[0] = 0;
}

/* frees the live sole-owner locals from index `from` up (they stay live: the caller decides about popping) */
static void emit_drops_except(JBuf *b, int from, int ind, void *keep) {
    if (!jl_drops || !L.cls || !L.cls->mem_on) return;
    for (int i = L.drop_n - 1; i >= from && i >= L.drop_base; i--) {
        if (L.drops[i] != keep) jl_emit_drop(b, ind, L.drops[i]->type, sname(L.drops[i]->name), jown_local_elems(L.drops[i]));
    }
}

static void emit_drops(JBuf *b, int from, int ind) { emit_drops_except(b, from, ind, NULL); }

static int ends_path(Node *s) {
    return s->kind == N_RETURN || s->kind == N_THROW || s->kind == N_BREAK || s->kind == N_CONTINUE;
}

void lblock_body(JBuf *b, Node *block, int ind) {
    if (block->kind != N_BLOCK) { ls(b, block, ind); return; }
    int mark = L.drop_n;
    for (int i = 0; i < block->l1.count; i++) {
        Node *s = block->l1.items[i];
        ls(b, s, ind);
        /* registered only here, so every entry is popped by the block that declared it */
        if (s->kind == N_LOCAL && s->sym && ((JLocal *)s->sym)->drop && jl_drop_ok(((JLocal *)s->sym)->type) && L.drop_n < 64) L.drops[L.drop_n++] = s->sym;
    }
    int n = block->l1.count;
    if (!n || !ends_path(block->l1.items[n - 1])) emit_drops(b, mark, ind);
    L.drop_n = mark;
}

static void lbody(JBuf *b, Node *s, int ind) { lblock_body(b, s, ind); }

/* A loop jown marked (region.c) rewinds the heap at the end of every iteration; `continue`, `break` and
 * `return` skip the rewind, which only leaves the memory allocated. */
static char *region_enter(JBuf *b, Node *loop, int ind) {
    if (!(jl_drops & JL_REGION) || !(loop->flags & JOWN_REGION_LOOP) || !L.cls || !L.cls->mem_on) return NULL;
    char *cp = new_temp();
    if (getenv("ASH_REGION_TRACE")) fprintf(stderr, "region loop: %s:%d\n", loop->path ? loop->path : "?", loop->line);
    emit_line(b, ind, "local %s = mem_region_enter();", cp);
    return cp;
}

static void region_leave(JBuf *b, Node *body, char *cp, int ind) {
    if (!cp) return;
    if (body->kind == N_BLOCK && body->l1.count && ends_path(body->l1.items[body->l1.count - 1])) return;
    emit_line(b, ind, "mem_region_leave(%s);", cp);
}

/* A loop condition is re-evaluated every iteration. When lowering it produced hoisted statements
 * (assignment/++ inside the condition, instanceof bindings), they are returned in *pre and the
 * loop is restructured: `during yes { pre; given not (cond) { stop; } body }`. */
static char *loop_cond(Node *c, char **pre) {
    size_t before = L.pre.len;
    char *t = lx(c);
    *pre = NULL;
    if (L.pre.len != before) {
        *pre = j_strdup(L.pre.data + before);
        L.pre.len = before;
        if (L.pre.data) L.pre.data[before] = 0;
    }
    return t;
}

static void each_line(JBuf *b, int ind, const char *text) {
    if (!text) return;
    const char *p = text;
    while (*p) {
        const char *nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);
        for (int i = 0; i < ind; i++) j_buf_add(b, "    ");
        j_buf_addn(b, p, len);
        j_buf_addc(b, '\n');
        if (!nl) break;
        p = nl + 1;
    }
}

static LoopCtx *real_loop(void) {
    LoopCtx *l = L.loop;
    while (l && l->is_switch) l = l->up;
    return l;
}

/* ---------------------------------------------------------------- switch */

static char *label_cond(Node *lb, const char *sel, JType *seltype) {
    (void)seltype;
    if (lb->kind == N_TYPEPATTERN) {
        if (lb->sym) alias_set(lb->sym, sel);
        JType *t = lb->type;
        if (t->kind == TY_CLASS) return j_fmt("__inst(%s, %s)", sel, quote_str(t->cls->qname));
        return "yes";
    }
    if (lb->kind == N_LIT_NULL) return j_fmt("(%s == none)", sel);
    if (lb->rk == R_ENUM_CONST && lb->sym) {
        JField *f = lb->sym;
        return j_fmt("(%s == %s[%s])", sel, cls_state(f->cls), quote_str(f->name));
    }
    return j_fmt("(%s == %s)", sel, lx(lb));
}

static char *case_cond(Node *c, const char *sel, JType *seltype) {
    if (c->l1.count == 0) return NULL; /* default */
    JBuf b = {0};
    for (int i = 0; i < c->l1.count; i++) {
        if (i) j_buf_add(&b, " or ");
        j_buf_add(&b, label_cond(c->l1.items[i], sel, seltype));
    }
    /* old Ash reads `given (a) or (b) {` as `given (a)` + junk: keep the whole test in one group */
    return j_fmt("(%s)", b.data);
}

/* Emits the if-chain; `expr_mode` makes expression arms `yield`. Returns nesting depth to close. */
static void switch_chain(JBuf *b, Node *n, const char *sel, int ind, int expr_mode) {
    JList order = {0};
    Node *dflt = NULL;
    for (int i = 0; i < n->l1.count; i++) {
        Node *c = n->l1.items[i];
        if ((c->flags & 1) && c->l1.count == 0) dflt = c;
        else j_list_push(&order, c);
    }
    int depth = 0;
    for (int i = 0; i < order.count; i++) {
        Node *c = order.items[i];
        char *cond = case_cond(c, sel, n->a->type);
        if ((c->flags & 1) && !dflt) dflt = c;
        flush_pre(b, ind + depth);
        emit_line(b, ind + depth, "given %s {", cond);
        if (c->str) {
            Node *body = c->b;
            if (expr_mode && body->kind == N_EXPRSTMT) {
                char *v = lx(body->a);
                flush_pre(b, ind + depth + 1);
                emit_line(b, ind + depth + 1, "yield %s;", v);
            } else {
                ls(b, body, ind + depth + 1);
            }
        } else {
            int cnt = c->l2.count;
            if (cnt && ((Node *)c->l2.items[cnt - 1])->kind == N_BREAK) cnt--;
            for (int k = 0; k < cnt; k++) ls(b, c->l2.items[k], ind + depth + 1);
            if (cnt == c->l2.count && cnt && i != order.count - 1 && !dflt) {
                Node *last = c->l2.items[cnt - 1];
                if (last->kind != N_RETURN && last->kind != N_THROW && last->kind != N_CONTINUE)
                    lerr(c, "switch fallthrough is not supported by the backend; end each case with break, return or throw");
            }
        }
        emit_line(b, ind + depth, "} otherwise {");
        depth++;
    }
    if (dflt) {
        if (dflt->str) {
            Node *body = dflt->b;
            if (expr_mode && body->kind == N_EXPRSTMT) {
                char *v = lx(body->a);
                flush_pre(b, ind + depth);
                emit_line(b, ind + depth, "yield %s;", v);
            } else {
                ls(b, body, ind + depth);
            }
        } else {
            int cnt = dflt->l2.count;
            if (cnt && ((Node *)dflt->l2.items[cnt - 1])->kind == N_BREAK) cnt--;
            for (int k = 0; k < cnt; k++) ls(b, dflt->l2.items[k], ind + depth);
        }
    } else if (expr_mode) {
        emit_line(b, ind + depth, "raise \"no switch case matched\";");
    }
    for (int d = depth - 1; d >= 0; d--) emit_line(b, ind + d, "}");
}

/* any `break` that targets this switch, looking through nested statements but not nested loops/switches */
static int contains_break(Node *n) {
    if (!n) return 0;
    switch (n->kind) {
        case N_BREAK: return 1;
        case N_BLOCK: case N_SEQ:
            for (int i = 0; i < n->l1.count; i++) if (contains_break(n->l1.items[i])) return 1;
            return 0;
        case N_IF: return contains_break(n->b) || contains_break(n->c);
        case N_TRY:
            if (contains_break(n->b) || contains_break(n->c)) return 1;
            for (int i = 0; i < n->l1.count; i++) if (contains_break(((Node *)n->l1.items[i])->b)) return 1;
            return 0;
        default: return 0;
    }
}

static int ends_the_case(Node *s) {
    return s->kind == N_BREAK || s->kind == N_RETURN || s->kind == N_THROW || s->kind == N_CONTINUE;
}

/* A colon-form switch needs the general lowering when a case falls through or breaks early. */
static int needs_wrapper(Node *n) {
    for (int i = 0; i < n->l1.count; i++) {
        Node *c = n->l1.items[i];
        if (c->str) return 0; /* arrow form never falls through */
        int cnt = c->l2.count;
        if (cnt && i != n->l1.count - 1 && !ends_the_case(c->l2.items[cnt - 1])) return 1;
        for (int k = 0; k < cnt - 1; k++) if (contains_break(c->l2.items[k])) return 1;
    }
    return 0;
}

/* General switch: pick the matching case index, then run bodies in source order from there until a
 * break (`stop` of a one-shot loop). Handles fallthrough, default in the middle and early breaks. */
static void lower_switch_general(JBuf *b, Node *n, char *sel, int ind) {
    char *m = new_temp();
    char *go = new_temp();
    int dflt = -1;
    emit_line(b, ind, "local %s = -1;", m);
    int depth = 0;
    for (int i = 0; i < n->l1.count; i++) {
        Node *c = n->l1.items[i];
        if (c->flags & 1) dflt = i;
        char *cond = case_cond(c, sel, n->a->type);
        if (!cond) continue;
        flush_pre(b, ind + depth);
        emit_line(b, ind + depth, "given %s {", cond);
        emit_line(b, ind + depth + 1, "%s = %d;", m, i);
        emit_line(b, ind + depth, "} otherwise {");
        depth++;
    }
    if (dflt >= 0) emit_line(b, ind + depth, "%s = %d;", m, dflt);
    for (int d = depth - 1; d >= 0; d--) emit_line(b, ind + d, "}");
    emit_line(b, ind, "local %s = no;", go);
    LoopCtx ctx = {0};
    ctx.drop_mark = L.drop_n;
    ctx.is_switch = 1;
    ctx.wrapper = 1;
    ctx.cont_flag = new_temp();
    ctx.up = L.loop;
    emit_line(b, ind, "local %s = no;", ctx.cont_flag);
    emit_line(b, ind, "during yes {");
    L.loop = &ctx;
    for (int i = 0; i < n->l1.count; i++) {
        Node *c = n->l1.items[i];
        emit_line(b, ind + 1, "given %s or %s == %d {", go, m, i);
        emit_line(b, ind + 2, "%s = yes;", go);
        for (int k = 0; k < c->l2.count; k++) ls(b, c->l2.items[k], ind + 2);
        emit_line(b, ind + 1, "}");
    }
    emit_line(b, ind + 1, "stop;");
    L.loop = ctx.up;
    emit_line(b, ind, "}");
    if (ctx.cont_used) {
        emit_line(b, ind, "given %s {", ctx.cont_flag);
        Node cont = {0};
        cont.kind = N_CONTINUE;
        ls(b, &cont, ind + 1);
        emit_line(b, ind, "}");
    }
}

static void lower_switch_stmt(JBuf *b, Node *n, int ind) {
    char *sel = new_temp();
    char *v = lx(n->a);
    flush_pre(b, ind);
    emit_line(b, ind, "local %s = %s;", sel, v);
    if (needs_wrapper(n)) {
        lower_switch_general(b, n, sel, ind);
        return;
    }
    LoopCtx ctx = {0};
    ctx.drop_mark = L.drop_n;
    ctx.is_switch = 1;
    ctx.up = L.loop;
    L.loop = &ctx;
    switch_chain(b, n, sel, ind, 0);
    L.loop = ctx.up;
}

char *lower_switch_expr(Node *n) {
    JBuf saved_pre = L.pre;
    memset(&L.pre, 0, sizeof L.pre);
    LoopCtx *saved_loop = L.loop;
    L.loop = NULL;
    JBuf body = {0};
    char *sel = new_temp();
    char *v = lx(n->a);
    flush_pre(&body, 1);
    emit_line(&body, 1, "local %s = %s;", sel, v);
    switch_chain(&body, n, sel, 1, 1);
    L.loop = saved_loop;
    L.pre = saved_pre;
    return j_fmt("__iife(forge() {\n%s})", body.data ? body.data : "");
}

/* ---------------------------------------------------------------- lambdas */

char *lower_lambda_body(Node *lam, JMethod *sam, int ind) {
    JBuf saved_pre = L.pre;
    memset(&L.pre, 0, sizeof L.pre);
    LoopCtx *saved_loop = L.loop;
    int saved_tf = L.fin_n;
    int saved_base = L.drop_base;
    L.drop_base = L.drop_n;
    L.loop = NULL;
    L.fin_n = 0;
    JBuf b = {0};
    j_buf_addc(&b, '\n');
    if (lam->b->kind == N_BLOCK) {
        lblock_body(&b, lam->b, ind + 1);
    } else {
        Node *e = lam->b;
        int void_ret = sam && sam->ret->kind == TY_VOID;
        if (e->kind == N_ASSIGN || e->kind == N_POSTFIX || (e->kind == N_UNARY && (e->name[1] == '+' || e->name[1] == '-'))) {
            emit_assign_stmt(&b, e, ind + 1);
        } else {
            char *v = lx(e);
            flush_pre(&b, ind + 1);
            if (void_ret) emit_line(&b, ind + 1, "%s;", v);
            else emit_line(&b, ind + 1, "yield %s;", v);
        }
    }
    L.loop = saved_loop;
    L.fin_n = saved_tf;
    L.drop_base = saved_base;
    L.pre = saved_pre;
    return b.data;
}

/* ---------------------------------------------------------------- try / catch */

static void lower_try(JBuf *b, Node *n, int ind) {
    char *fin = NULL;
    int base = ind;
    if (n->c) {
        fin = new_temp();
        emit_line(b, ind, "local %s = none;", fin);
        emit_line(b, ind, "attempt {");
        base = ind + 1;
    }
    if (n->c) {
        if (L.fin_n >= 16) lerr(n, "try/finally nested too deeply");
        else L.fin[L.fin_n++] = n->c;
    }
    if (n->l1.count == 0) {
        lbody(b, n->b, base);
    } else {
        char *ev = new_temp();
        emit_line(b, base, "attempt {");
        lbody(b, n->b, base + 1);
        emit_line(b, base, "} handle (%s) {", ev);
        emit_line(b, base + 1, "%s = __wrap(%s);", ev, ev);
        int depth = 0;
        for (int i = 0; i < n->l1.count; i++) {
            Node *c = n->l1.items[i];
            JBuf cond = {0};
            for (int j = 0; j < c->l1.count; j++) {
                Node *tn = c->l1.items[j];
                if (j) j_buf_add(&cond, " or ");
                if (tn->type && tn->type->kind == TY_CLASS) j_buf_addf(&cond, "__inst(%s, %s)", ev, quote_str(tn->type->cls->qname));
                else j_buf_add(&cond, "no");
            }
            emit_line(b, base + 1 + depth, "given %s {", cond.data ? cond.data : "no");
            emit_line(b, base + 2 + depth, "local %s = %s;", sname(c->name), ev);
            lbody(b, c->b, base + 2 + depth);
            emit_line(b, base + 1 + depth, "} otherwise {");
            depth++;
        }
        emit_line(b, base + 1 + depth, "raise %s;", ev);
        for (int d = depth - 1; d >= 0; d--) emit_line(b, base + 1 + d, "}");
        emit_line(b, base, "}");
    }
    if (n->c && L.fin_n > 0) L.fin_n--;
    if (n->c) {
        char *xv = new_temp();
        emit_line(b, ind, "} handle (%s) {", xv);
        emit_line(b, ind + 1, "%s = %s;", fin, xv);
        emit_line(b, ind, "}");
        lbody(b, n->c, ind);
        emit_line(b, ind, "given %s != none {", fin);
        emit_line(b, ind + 1, "raise %s;", fin);
        emit_line(b, ind, "}");
    }
}

/* ---------------------------------------------------------------- statements */

static int native_iterable(JType *t) {
    return t->kind == TY_ARRAY || is_array_list(t);
}

static char *ex_new_cme(void) {
    JMethod *k = find_ctor(jt_well_known("ash.util.ConcurrentModificationException"), 0);
    return k ? fn_name(k, "new") : (char *)"__missing_ctor";
}

static void lower_foreach(JBuf *b, Node *n, int ind) {
    char *it = lx(n->b);
    char *name = sname(n->name);
    LoopCtx ctx = {0};
    ctx.drop_mark = L.drop_n;
    ctx.up = L.loop;
    flush_pre(b, ind);
    if (n->b->type->kind == TY_ARRAY || (native_iterable(n->b->type) && foreach_body_keeps_sizes(n->c))) {
        /* an array cannot change size; neither can a list that a body without project code and without list
         * mutators walks, so the check below is left out and the loop is the plain, fastest one */
        emit_line(b, ind, "each %s in %s {", name, it);
    } else if (native_iterable(n->b->type)) {
        /* Java's ArrayList iterator: the loop ends when the cursor reaches the size, and a size that changed
         * since the loop began is a ConcurrentModificationException when the next element is asked for. The
         * list is read by index on every iteration, so neither backend can walk a stale data pointer. */
        char *lv = new_temp(), *nv = new_temp(), *iv = new_temp();
        emit_line(b, ind, "local %s = %s;", lv, it);
        emit_line(b, ind, "local %s = len(%s);", nv, lv);
        emit_line(b, ind, "local %s = 0;", iv);
        emit_line(b, ind, "during not (%s == len(%s)) {", iv, lv);
        emit_line(b, ind + 1, "given not (len(%s) == %s) { raise %s(); }", lv, nv, ex_new_cme());
        emit_line(b, ind + 1, "local %s = %s[%s];", name, lv, iv);
        emit_line(b, ind + 1, "%s += 1;", iv);
    } else {
        char *iv = new_temp();
        emit_line(b, ind, "local %s = __iterof(%s);", iv, it);
        emit_line(b, ind, "during __vc0(%s, \"hasNext/0\") {", iv);
        emit_line(b, ind + 1, "local %s = __vc0(%s, \"next/0\");", name, iv);
    }
    L.loop = &ctx;
    /* the loop variable is bound by `each` before the checkpoint, so it survives the rewind */
    char *rg = region_enter(b, n, ind + 1);
    lbody(b, n->c, ind + 1);
    region_leave(b, n->c, rg, ind + 1);
    L.loop = ctx.up;
    emit_line(b, ind, "}");
}

static char *update_text(JList *ups) {
    JBuf b = {0};
    for (int i = 0; i < ups->count; i++) {
        Node *u = ups->items[i];
        if (u->kind == N_ASSIGN || u->kind == N_POSTFIX || (u->kind == N_UNARY && (u->name[0] == '+' || u->name[0] == '-') && u->name[1])) {
            emit_assign_stmt(&b, u, 0);
        } else {
            char *v = lx(u);
            flush_pre(&b, 0);
            emit_line(&b, 0, "%s;", v);
        }
    }
    return b.data ? b.data : (char *)"";
}

/* `for (...; i < n; i++)` cannot overflow i: the comparison bounds it, so the update skips the 32-bit wrap */
static void mark_bounded_counter(Node *n) {
    if (!n->a || n->a->kind != N_BINARY || n->a->a->kind != N_NAME || n->a->a->rk != R_LOCAL) return;
    const char *op = n->a->name;
    int up = !strcmp(op, "<") || !strcmp(op, "<=");
    int down = !strcmp(op, ">") || !strcmp(op, ">=");
    if (!up && !down) return;
    for (int i = 0; i < n->l2.count; i++) {
        Node *u = n->l2.items[i];
        if ((u->kind == N_POSTFIX || (u->kind == N_UNARY && u->name[1])) && u->a->kind == N_NAME && u->a->sym == n->a->a->sym) {
            int inc = u->name[0] == '+';
            if ((up && inc) || (down && !inc)) u->flags |= 0x200;
        }
    }
}

static void lower_for(JBuf *b, Node *n, int ind) {
    mark_bounded_counter(n);
    emit_line(b, ind, "given yes {");
    for (int i = 0; i < n->l1.count; i++) ls(b, n->l1.items[i], ind + 1);
    char *pre = NULL;
    char *cond = n->a ? loop_cond(n->a, &pre) : "yes";
    flush_pre(b, ind + 1);
    LoopCtx ctx = {0};
    ctx.drop_mark = L.drop_n;
    ctx.up = L.loop;
    ctx.update = update_text(&n->l2);
    if (pre) {
        emit_line(b, ind + 1, "during yes {");
        each_line(b, ind + 2, pre);
        emit_line(b, ind + 2, "given not %s {", cond);
        emit_line(b, ind + 3, "stop;");
        emit_line(b, ind + 2, "}");
    } else {
        emit_line(b, ind + 1, "during %s {", cond);
    }
    L.loop = &ctx;
    char *rg = region_enter(b, n, ind + 2);
    lbody(b, n->b, ind + 2);
    region_leave(b, n->b, rg, ind + 2);
    L.loop = ctx.up;
    each_line(b, ind + 2, ctx.update);
    emit_line(b, ind + 1, "}");
    emit_line(b, ind, "}");
}

static void lower_local(JBuf *b, Node *n, int ind) {
    char *init = n->b ? lx(n->b) : "none";
    flush_pre(b, ind);
    emit_line(b, ind, "local %s = %s;", sname(n->name), init);
}

void ls(JBuf *b, Node *s, int ind) {
    switch (s->kind) {
        case N_BLOCK:
            emit_line(b, ind, "given yes {");
            lblock_body(b, s, ind + 1);
            emit_line(b, ind, "}");
            break;
        case N_SEQ: for (int i = 0; i < s->l1.count; i++) ls(b, s->l1.items[i], ind); break;
        case N_EMPTY: break;
        case N_LOCAL: lower_local(b, s, ind); break;
        case N_EXPRSTMT: {
            Node *e = s->a;
            if (e->kind == N_ASSIGN || e->kind == N_POSTFIX || (e->kind == N_UNARY && (e->name[0] == '+' || e->name[0] == '-') && e->name[1])) {
                emit_assign_stmt(b, e, ind);
            } else {
                char *v = lx(e);
                flush_pre(b, ind);
                emit_line(b, ind, "%s;", v);
            }
            break;
        }
        case N_IF: {
            char *c = lx(s->a);
            flush_pre(b, ind);
            emit_line(b, ind, "given %s {", c);
            lbody(b, s->b, ind + 1);
            if (s->c) {
                emit_line(b, ind, "} otherwise {");
                lbody(b, s->c, ind + 1);
            }
            emit_line(b, ind, "}");
            break;
        }
        case N_WHILE: {
            char *pre;
            char *c = loop_cond(s->a, &pre);
            LoopCtx ctx = {0};
            ctx.drop_mark = L.drop_n;
    ctx.drop_mark = L.drop_n;
            ctx.up = L.loop;
            if (pre) {
                emit_line(b, ind, "during yes {");
                each_line(b, ind + 1, pre);
                emit_line(b, ind + 1, "given not %s {", c);
                emit_line(b, ind + 2, "stop;");
                emit_line(b, ind + 1, "}");
            } else {
                emit_line(b, ind, "during %s {", c);
            }
            L.loop = &ctx;
            char *rg = region_enter(b, s, ind + 1);
            lbody(b, s->b, ind + 1);
            region_leave(b, s->b, rg, ind + 1);
            L.loop = ctx.up;
            emit_line(b, ind, "}");
            break;
        }
        case N_DO: {
            char *first = new_temp();
            char *pre;
            char *c = loop_cond(s->a, &pre);
            LoopCtx ctx = {0};
            ctx.drop_mark = L.drop_n;
    ctx.drop_mark = L.drop_n;
            ctx.up = L.loop;
            emit_line(b, ind, "local %s = yes;", first);
            if (pre) {
                emit_line(b, ind, "during yes {");
                emit_line(b, ind + 1, "given not %s {", first);
                each_line(b, ind + 2, pre);
                emit_line(b, ind + 2, "given not %s {", c);
                emit_line(b, ind + 3, "stop;");
                emit_line(b, ind + 2, "}");
                emit_line(b, ind + 1, "}");
            } else {
                emit_line(b, ind, "during %s or %s {", first, c);
            }
            emit_line(b, ind + 1, "%s = no;", first);
            L.loop = &ctx;
            char *rg = region_enter(b, s, ind + 1);
            lbody(b, s->b, ind + 1);
            region_leave(b, s->b, rg, ind + 1);
            L.loop = ctx.up;
            emit_line(b, ind, "}");
            break;
        }
        case N_FOR: lower_for(b, s, ind); break;
        case N_FOREACH: lower_foreach(b, s, ind); break;
        case N_RETURN: {
            char *v = s->a ? lx(s->a) : "none";
            flush_pre(b, ind);
            int drops = jl_drops && L.cls && L.cls->mem_on && L.drop_n > L.drop_base;
            if (drops || L.fin_n) {
                /* the value may read a local that is about to be freed, and finally bodies run before the
                 * function leaves: compute the value first, then finally, then free */
                char *rv = new_temp();
                emit_line(b, ind, "local %s = %s;", rv, v);
                int saved = L.fin_n;
                for (int i = saved - 1; i >= 0; i--) {
                    L.fin_n = i;
                    lbody(b, L.fin[i], ind);
                }
                L.fin_n = saved;
                /* `return x` hands x to the caller */
                if (drops) emit_drops_except(b, 0, ind, s->a && s->a->kind == N_NAME && s->a->rk == R_LOCAL ? s->a->sym : NULL);
                v = rv;
            }
            emit_line(b, ind, "yield %s;", v);
            break;
        }
        case N_BREAK: {
            if (L.loop && L.loop->is_switch && !L.loop->wrapper) {
                lerr(s, "break inside a switch must be the last statement of its case");
                break;
            }
            if (L.loop && !L.fin_n) emit_drops(b, L.loop->drop_mark, ind);   /* not under a finally: it may still read them */
            emit_line(b, ind, "stop;");
            break;
        }
        case N_CONTINUE: {
            if (L.loop && L.loop->is_switch && L.loop->wrapper && real_loop()) {
                /* leave the one-shot switch loop first, then continue the real loop */
                L.loop->cont_used = 1;
                emit_line(b, ind, "%s = yes;", L.loop->cont_flag);
                emit_line(b, ind, "stop;");
                break;
            }
            LoopCtx *l = real_loop();
            if (l && !L.fin_n) emit_drops(b, l->drop_mark, ind);
            if (l && l->update) each_line(b, ind, l->update), (void)0;
            emit_line(b, ind, "next;");
            break;
        }
        case N_THROW: {
            char *v = lx(s->a);
            flush_pre(b, ind);
            emit_line(b, ind, "raise %s;", v);
            break;
        }
        case N_TRY: lower_try(b, s, ind); break;
        case N_SWITCH: lower_switch_stmt(b, s, ind); break;
        case N_CTORCALL: {
            char *t = lower_ctor_call(s);
            flush_pre(b, ind);
            if (*t) emit_line(b, ind, "%s", t);
            break;
        }
        default:
            lerr(s, "cannot lower this statement");
    }
}
