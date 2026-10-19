#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jcommon/H/jcommon.h"
#include "jown/H/own.h"
#include "mnemo/H/flow/flow.h"
#include "jown/H/range.h"
#include "jown/H/region.h"
#include "jtypes/H/check.h"
#include "jtypes/H/jtypes.h"

/* ---------------------------------------------------------------- project flag */

/* Finds the nearest ash.pkg at or above the entry file. Returns its directory (arena string) or NULL. */
static char *find_project_root(const char *entry_path) {
    char *dir = j_strdup(entry_path);
    char *slash = strrchr(dir, '/');
    if (slash) *slash = 0; else strcpy(dir, ".");
    char *entry_dir = j_strdup(dir);
    for (;;) {
        char *pkg = j_fmt("%s/ash.pkg", dir);
        FILE *f = fopen(pkg, "r");
        if (f) { fclose(f); return dir; }
        slash = strrchr(dir, '/');
        if (!slash || slash == dir) break;
        *slash = 0;
    }
    return entry_dir;      /* no ash.pkg: the entry file's directory is the project, with the default setting */
}

static int memory_safety_on(const char *root) {
    char *text = j_read_file(j_fmt("%s/ash.pkg", root));
    if (!text) return JOWN_DEFAULT_ON;
    for (char *line = strtok(text, "\n"); line; line = strtok(NULL, "\n")) {
        if (line[0] == '[') break;                       /* the flat header ends at the first section */
        if (strncmp(line, "memory-safety:", 14) != 0) continue;
        char *v = line + 14;
        while (*v == ' ' || *v == '\t') v++;
        return strncmp(v, "off", 3) != 0;
    }
    return JOWN_DEFAULT_ON;
}

/* ---------------------------------------------------------------- flow state (kept in mnemo.a) */

typedef struct Target {
    struct Target *up;
    int is_switch;     /* `break` ends a switch; `continue` belongs to the nearest loop */
    uint32_t brk, cont; /* mnemo snapshots accumulating the states that leave through break / continue */
    int nfin;          /* finally blocks open when the target began: a jump out of it runs the ones above */
} Target;

typedef struct {
    MnemoWalk *m;
    int in_lambda;     /* inside a lambda body: it runs later, so what is assigned by then is unknown */
    Target *targets;
    JList reported;    /* Node* already diagnosed, so the second loop pass stays quiet */
    JList fins;        /* finally blocks of the try statements being walked, innermost last */
    RgCtx rg;          /* value-range facts (range.c); shares the MnemoWalk */
    int prefix;        /* in the first pass of an outermost loop, on the straight-line part of its first iteration */
    int catch_depth;   /* inside a try block that has a catch */
    int loop_depth;    /* loops being walked */
    int passes;        /* loop passes walked in this method, against LOOP_PASS_BUDGET */
} Walk;

static uint64_t id_of(JLocal *l) { return (uint64_t)(uintptr_t)l; }

static int owning(JType *t) {
    if (!t) return 0;
    t = ty_unbox(t);
    if (t->kind == TY_ARRAY || t->kind == TY_TVAR) return 1;
    if (t->kind != TY_CLASS) return 0;
    return t->cls && t->cls->kind != N_ENUM;
}

/* ---------------------------------------------------------------- expressions */

static int param_borrowed(JMethod *m, int i);
static int fixed_project_target(JMethod *m);
static void expr(Walk *w, Node *n);
static void stmt(Walk *w, Node *n);

/* A local declared without an initializer is tracked with two more ids in the moved set (a pointer plus 1 and
 * plus 2 never collides with another local): +1 marks it as declared that way, +2 means "some path assigned it".
 * The set joins as a union, so a use where +2 is missing is a use where *no* path assigned it: it can only read
 * the empty value. Paths that merely might leave it unassigned are not reported. */
static uint64_t decl_id(JLocal *l) { return id_of(l) + 1; }
static uint64_t set_id(JLocal *l) { return id_of(l) + 2; }

static void mark_assigned(Walk *w, JLocal *l) { mnemo_move(w->m, set_id(l)); }

/* A loop's first pass starts from the state on entry, so what it sees on the straight-line part of the first
 * iteration (statements that every run of the body executes before any branch, jump or inner loop) is exact:
 * a variable no path assigned by then is read unassigned there, even if a later statement of the body assigns it
 * for the next iteration. A use behind a condition is not judged that way (`if (!first) print(c);`). */
static void use(Walk *w, Node *n) {
    JLocal *l = n->sym;
    int first_iter = w->rg.mute && w->prefix && !w->catch_depth;
    if (l && !w->in_lambda && (!w->rg.mute || first_iter) && mnemo_is_moved(w->m, decl_id(l)) && !mnemo_is_moved(w->m, set_id(l))) {
        int dup = 0;
        for (int i = 0; i < w->reported.count; i++) if (w->reported.items[i] == n) dup = 1;
        if (!dup) {
            j_list_push(&w->reported, n);
            if (first_iter) cerr(n, "variable '%s' is used before it is assigned: the first pass through the loop reaches this use before any assignment; give it a value before the loop (or where it is declared)", n->name);
            else cerr(n, "variable '%s' is used before it is assigned: no path assigns it before this use; give it a value where it is declared, or assign it first", n->name);
        }
        mark_assigned(w, l);                          /* one report per variable and path */
        return;
    }
    if (!l || !mnemo_is_moved(w->m, id_of(l))) return;
    for (int i = 0; i < w->reported.count; i++) if (w->reported.items[i] == n) return;
    j_list_push(&w->reported, n);
    cerr(n, "use of moved value '%s': it was moved earlier by an assignment or by passing it to a call; lend it with &%s, clone it, or assign a new value first", n->name, n->name);
}

/* evaluates `e` as a value that is being stored: a bare owning local is moved out of */
static void store(Walk *w, Node *e) {
    expr(w, e);
    if (e->kind == N_NAME && e->rk == R_LOCAL && e->sym && owning(((JLocal *)e->sym)->type)) {
        mnemo_move(w->m, id_of(e->sym));
    }
}

/* ---- calls: arguments are moved into project code unless written `&x` / `&mut x` ---- */

typedef struct { JLocal *l; int mut; Node *at; } Borrow;   /* a local named by a call's receiver or arguments */

static int is_borrow(Node *a) {
    return a->kind == N_UNARY && a->name[0] == '&' && a->a->kind == N_NAME && a->a->rk == R_LOCAL && a->a->sym;
}

/* project code owns what it is given; library and native code only looks at it */
static int callee_takes(JMethod *m) {
    /* equals / compareTo only compare: they never consume their argument */
    if (m && (!strcmp(m->name, "equals") || !strcmp(m->name, "compareTo"))) return 0;
    return m && !m->tmpl && m->cls && m->cls->mem_on;
}

static void call_args(Walk *w, Node *n, JMethod *m, Node *recv, JList *args) {
    Borrow seen[32];
    int ns = 0;
    if (m && m->tmpl && !strncmp(m->tmpl, "mem_release(", 12)) {
        int dup = 0;
        for (int k = 0; k < w->reported.count; k++) if (w->reported.items[k] == n) dup = 1;
        if (!dup) {
            j_list_push(&w->reported, n);
            cerr(n, "System.free is only available with 'memory-safety: off' in ash.pkg: with it on, memory is freed for you");
        }
    }
    if (recv) {
        expr(w, recv);
        if (recv->kind == N_NAME && recv->rk == R_LOCAL && recv->sym) seen[ns++] = (Borrow){recv->sym, 0, recv};
    }
    int takes = callee_takes(m);
    for (int i = 0; i < args->count; i++) {
        Node *a = args->items[i];
        if (is_borrow(a)) {
            if (a->name[1] == 'm') mark_assigned(w, a->a->sym);          /* the callee may fill it in */
            expr(w, a->a);
            /* a borrow may not outlive the call: a callee we can see that keeps its parameter cannot be lent to */
            if (fixed_project_target(m) && !param_borrowed(m, i) && !(m->decl && m->decl->b == NULL)) {
                int dup = 0;
                for (int k = 0; k < w->reported.count; k++) if (w->reported.items[k] == a) dup = 1;
                if (!dup) {
                    j_list_push(&w->reported, a);
                    cerr(a, "cannot lend '%s' with '%s': '%s' keeps this parameter (stores, returns or passes it on); pass '%s' by move or clone it",
                         a->a->name, a->name, m->name, a->a->name);
                }
            }
            if (a->name[1] == 'm') rg_forget(&w->rg, a->a->sym);        /* the callee may write it */
            if (ns < 32) seen[ns++] = (Borrow){a->a->sym, a->name[1] == 'm', a};
        } else if (takes && !param_borrowed(m, i)) {
            store(w, a);
            if (a->kind == N_NAME && a->rk == R_LOCAL && a->sym && ns < 32) seen[ns++] = (Borrow){a->sym, 0, a};
        } else {
            expr(w, a);
            if (a->kind == N_NAME && a->rk == R_LOCAL && a->sym && ns < 32) seen[ns++] = (Borrow){a->sym, 0, a};
        }
    }
    /* Rust's rule: an exclusive borrow excludes every other use of the same variable in the call */
    for (int i = 0; i < ns; i++) {
        for (int j = i + 1; j < ns; j++) {
            if (seen[i].l != seen[j].l || (!seen[i].mut && !seen[j].mut)) continue;
            for (int k = 0; k < w->reported.count; k++) if (w->reported.items[k] == seen[j].at) goto next_pair;
            j_list_push(&w->reported, seen[j].at);
            cerr(seen[j].at, "cannot use '%s' here: it is already %s in this call", seen[i].l->name,
                 seen[i].mut ? "borrowed mutably" : "borrowed, and this use is mutable");
        next_pair:;
        }
    }
    (void)n;
}

static void children(Walk *w, Node *n) {
    if (n->a) expr(w, n->a);
    if (n->b) expr(w, n->b);
    if (n->c) expr(w, n->c);
    JList *ls[] = {&n->l1, &n->l2, &n->l3, &n->l4, &n->l5};
    for (int k = 0; k < 5; k++) for (int i = 0; i < ls[k]->count; i++) expr(w, ls[k]->items[i]);
}

static void expr(Walk *w, Node *n) {
    MnemoWalk *m = w->m;
    switch (n->kind) {
        case N_NAME:
            if (n->rk == R_LOCAL) use(w, n);
            return;
        case N_ASSIGN:
            if (!strcmp(n->name, "=")) {
                int to_local = n->a->kind == N_NAME && n->a->rk == R_LOCAL && n->a->sym;
                if (!to_local) expr(w, n->a);               /* Java evaluates the array and the index before the right side */
                store(w, n->b);
                if (to_local) { mnemo_reinit(m, id_of(n->a->sym)); mark_assigned(w, n->a->sym); }
            } else {
                expr(w, n->a);
                expr(w, n->b);
            }
            rg_assign(&w->rg, n);
            return;
        case N_ARRAYINIT:
            for (int i = 0; i < n->l1.count; i++) store(w, n->l1.items[i]);
            return;
        case N_CALL: call_args(w, n, n->sym, n->a, &n->l1); rg_check_call(&w->rg, n); rg_call_effect(&w->rg, n); return;
        case N_NEW: call_args(w, n, n->sym, NULL, &n->l1); return;
        case N_UNARY:
            if (n->name[0] == '&') {
                cerr(n, "a borrow ('&%s') can only be passed as a call argument; it cannot be stored, returned or assigned, so pass '%s' by move or clone it", n->a && n->a->name ? n->a->name : "x", n->a && n->a->name ? n->a->name : "x");
                return;
            }
            children(w, n);
            if ((n->name[0] == '+' && n->name[1] == '+') || (n->name[0] == '-' && n->name[1] == '-')) rg_incdec(&w->rg, n);
            return;
        case N_POSTFIX:
            children(w, n);
            rg_incdec(&w->rg, n);
            return;
        case N_INDEX: children(w, n); rg_check_index(&w->rg, n); return;
        case N_NEWARRAY: children(w, n); rg_check_newarray(&w->rg, n); return;
        case N_BINARY:
            if (!strcmp(n->name, "&&") || !strcmp(n->name, "||")) {   /* right side may not run */
                expr(w, n->a);
                int pfx = w->prefix;
                w->prefix = 0;                                 /* the right side may not run */
                uint32_t after_a = mnemo_save(m);
                rg_assume(&w->rg, n->a, n->name[0] == '&');
                expr(w, n->b);
                w->prefix = pfx;
                mnemo_join(m, after_a);
                mnemo_release(m, after_a);
                return;
            }
            children(w, n);
            rg_check_binary(&w->rg, n);
            return;
        case N_COND: {
            expr(w, n->a);
            int pfx = w->prefix;
            w->prefix = 0;                                     /* one of the two arms runs */
            uint32_t entry = mnemo_save(m);
            rg_assume(&w->rg, n->a, 1);
            expr(w, n->b);
            uint32_t then_s = mnemo_save(m);
            mnemo_restore(m, entry);
            rg_assume(&w->rg, n->a, 0);
            expr(w, n->c);
            mnemo_join(m, then_s);
            mnemo_release(m, entry);
            mnemo_release(m, then_s);
            w->prefix = pfx;
            return;
        }
        case N_INSTANCEOF:
            children(w, n);
            if (n->sym) mnemo_reinit(m, id_of(n->sym));       /* the binding is a fresh variable each time the test runs */
            return;
        case N_SWITCH: {                     /* a switch expression: each case starts from the state after the selector */
            int pfx = w->prefix;
            stmt(w, n);
            w->prefix = pfx;
            return;
        }
        case N_LAMBDA: {                     /* the body runs later: check its uses, keep its moves local */
            int pfx = w->prefix;
            w->prefix = 0;
            uint32_t entry = mnemo_save(m);
            Target *saved = w->targets;
            w->targets = NULL;
            w->in_lambda++;
            expr(w, n->b);
            w->in_lambda--;
            w->targets = saved;
            w->prefix = pfx;
            mnemo_restore(m, entry);
            mnemo_release(m, entry);
            return;
        }
        default:
            break;
    }
    children(w, n);
}

/* ---------------------------------------------------------------- statements */

static void stmts(Walk *w, JList *l) {
    for (int i = 0; i < l->count; i++) stmt(w, l->items[i]);
}

static Target *find_target(Walk *w, int for_continue) {
    for (Target *t = w->targets; t; t = t->up) if (!for_continue || !t->is_switch) return t;
    return NULL;
}

/* `break` / `continue` run the finally blocks of the try statements they leave before the jump lands, so the
 * state that reaches the target is the one after them (a `finally { x = 5; }` changes what is known of x). */
static void jump(Walk *w, int is_continue) {
    Target *t = find_target(w, is_continue);
    if (t) {
        int open = w->fins.count, n = open - t->nfin;
        if (n > 0) {
            Node **mine = malloc((size_t)n * sizeof *mine);      /* the walks below push and pop on the same list */
            if (mine) {
                for (int i = 0; i < n; i++) mine[i] = w->fins.items[t->nfin + i];
                for (int i = n - 1; i >= 0 && !mnemo_is_dead(w->m); i--) {
                    w->fins.count = t->nfin + i;               /* a jump inside the finally leaves only the ones below it */
                    stmt(w, mine[i]);
                }
                w->fins.count = open;
                for (int i = 0; i < n; i++) w->fins.items[t->nfin + i] = mine[i];
                free(mine);
            } else {
                mnemo_havoc(w->m);                              /* out of memory: give up the ranges, they may be stale */
            }
        }
        mnemo_join_into(w->m, is_continue ? t->cont : t->brk);
    }
    mnemo_kill(w->m);
}

static void push_target(Walk *w, Target *t, int is_switch) {
    t->up = w->targets;
    t->is_switch = is_switch;
    t->nfin = w->fins.count;
    t->brk = mnemo_save_dead(w->m);
    t->cont = mnemo_save_dead(w->m);
    w->targets = t;
}

static void pop_target(Walk *w, Target *t) {
    w->targets = t->up;
    mnemo_release(w->m, t->brk);
    mnemo_release(w->m, t->cont);
}

/* Nested loops are walked again for every pass of the loop around them, so a nest of d loops costs about 2^d
 * walks. Real code stays far below this many passes in a method; past it (a pathological nest) the loops that are
 * only searching for the outer fixpoint stop after one pass and give up their ranges, which keeps the cost linear
 * and only costs precision (fewer range errors are certain), never soundness. */
#define LOOP_PASS_BUDGET 2000

/* A loop is walked until its head state stops changing, then once more with the diagnostics on. The head
 * is the entry state joined with every back edge: moved variables only grow, and a value range that
 * grows is widened to infinity, so this takes a few passes. A condition narrows the ranges for the body
 * and for the exit. A loop that never settles forgets its ranges rather than guessing. */
static void loop(Walk *w, Node *n) {
    MnemoWalk *m = w->m;
    Target t;
    push_target(w, &t, 0);
    if (n->kind == N_FOREACH) expr(w, n->b);          /* the iterable is evaluated once */
    uint32_t head = mnemo_save(m);
    uint32_t leave = mnemo_save_dead(m);              /* states that leave through the condition */
    int final = 0;
    int outermost = w->loop_depth == 0;
    /* inside a pass of an enclosing loop that is itself only searching for its fixpoint nothing is reported, so
     * this loop need not walk once more with the diagnostics on: stopping as soon as its head is stable already
     * gives the exit state, and a nest of d loops costs 2^d walks instead of 3^d */
    int searching = w->rg.mute > 0;
    w->loop_depth++;
    for (int pass = 0; pass < 10; pass++) {
        if (!final) w->rg.mute++;
        w->prefix = pass == 0 && outermost;
        w->passes++;
        mnemo_restore(m, head);
        if (n->kind == N_DO) {
            stmt(w, n->b);
            mnemo_join(m, t.cont);
            if (n->a) {
                expr(w, n->a);
                uint32_t s = mnemo_save(m);
                rg_assume(&w->rg, n->a, 0);
                mnemo_join_into(m, leave);
                mnemo_restore(m, s);
                mnemo_release(m, s);
                rg_assume(&w->rg, n->a, 1);
            } else {
                mnemo_join_into(m, leave);
            }
        } else {
            if (n->kind == N_FOREACH) {
                if (n->sym) { mnemo_reinit(m, id_of(n->sym)); rg_forget(&w->rg, n->sym); }
                mnemo_join_into(m, leave);
            } else if (n->a) {
                expr(w, n->a);
                uint32_t s = mnemo_save(m);
                rg_assume(&w->rg, n->a, 0);
                mnemo_join_into(m, leave);
                mnemo_restore(m, s);
                mnemo_release(m, s);
                rg_assume(&w->rg, n->a, 1);
            }                                         /* `for (;;)` leaves only through break */
            stmt(w, n->kind == N_FOREACH ? n->c : n->b);
            mnemo_join(m, t.cont);
            if (n->kind == N_FOR) for (int i = 0; i < n->l2.count; i++) expr(w, n->l2.items[i]);
        }
        w->prefix = 0;
        if (!final) w->rg.mute--;
        if (final) break;
        uint32_t end = mnemo_save(m);
        mnemo_restore(m, head);
        mnemo_join_widen(m, end);                     /* back edge */
        mnemo_release(m, end);
        if (searching && w->passes > LOOP_PASS_BUDGET) {
            /* The moved set of the head is a fixpoint after one pass (a variable moves or is re-initialised on a
             * path whatever the head holds), so only the ranges are unknown: forget them, and let this head stand
             * for every state that leaves the loop. */
            mnemo_havoc(m);
            mnemo_join_into(m, leave);
            mnemo_join_into(m, t.brk);
            mnemo_release(m, head);
            head = mnemo_save(m);
            break;
        }
        int settled = mnemo_equal(m, head);
        if (!settled && pass >= 8) { mnemo_havoc(m); settled = 1; }
        mnemo_release(m, head);
        head = mnemo_save(m);
        if (settled && searching) break;
        if (settled && pass >= 1) final = 1;
    }
    w->loop_depth--;
    mnemo_restore(m, leave);
    mnemo_join(m, t.brk);
    mnemo_release(m, head);
    mnemo_release(m, leave);
    pop_target(w, &t);
}

/* a statement that always runs to its end: blocks, declarations, expression statements and ifs of those */
static int runs_through(Node *n) {
    if (!n) return 1;
    switch (n->kind) {
        case N_BLOCK: case N_SEQ:
            for (int i = 0; i < n->l1.count; i++) if (!runs_through(n->l1.items[i])) return 0;
            return 1;
        case N_LOCAL: case N_EXPRSTMT: case N_EMPTY: return 1;
        case N_IF: return runs_through(n->b) && runs_through(n->c);
        default: return 0;
    }
}

static void stmt(Walk *w, Node *n) {
    MnemoWalk *m = w->m;
    switch (n->kind) {         /* only these keep the straight-line part going (an `if` after its condition) */
        case N_BLOCK: case N_SEQ: case N_LOCAL: case N_EXPRSTMT: case N_EMPTY: case N_IF: case N_RETURN: case N_THROW: break;
        default: w->prefix = 0;
    }
    switch (n->kind) {
        case N_BLOCK: case N_SEQ: stmts(w, &n->l1); return;
        case N_LOCAL:
            if (n->b) store(w, n->b);
            if (n->sym) mnemo_reinit(m, id_of(n->sym));   /* a fresh variable, also on every loop iteration */
            if (n->sym && !n->b) { mnemo_move(m, decl_id(n->sym)); mnemo_reinit(m, set_id(n->sym)); }
            else if (n->sym) mnemo_reinit(m, decl_id(n->sym));
            rg_declare(&w->rg, n);
            return;
        case N_EXPRSTMT: expr(w, n->a); return;
        case N_IF: {
            expr(w, n->a);
            int pfx = w->prefix && runs_through(n);     /* the rest still runs after an if without jumps */
            w->prefix = 0;                             /* the branches are conditional */
            uint32_t entry = mnemo_save(m);
            rg_assume(&w->rg, n->a, 1);
            stmt(w, n->b);
            uint32_t then_s = mnemo_save(m);
            mnemo_restore(m, entry);
            rg_assume(&w->rg, n->a, 0);
            if (n->c) stmt(w, n->c);
            mnemo_join(m, then_s);
            mnemo_release(m, entry);
            mnemo_release(m, then_s);
            w->prefix = pfx;
            return;
        }
        case N_WHILE: case N_DO: case N_FOR: case N_FOREACH:
            if (n->kind == N_FOR) stmts(w, &n->l1);
            loop(w, n);
            return;
        case N_RETURN: if (n->a) expr(w, n->a); w->prefix = 0; mnemo_kill(m); return;
        case N_THROW: expr(w, n->a); w->prefix = 0; mnemo_kill(m); return;
        case N_BREAK: jump(w, 0); return;
        case N_CONTINUE: jump(w, 1); return;
        case N_TRY: {
            uint32_t entry = mnemo_save(m);
            if (n->l1.count) { w->rg.mute++; w->catch_depth++; }   /* a catch may be the point: the program can provoke the exception */
            if (n->c) j_list_push(&w->fins, n->c);
            stmt(w, n->b);
            if (n->l1.count) { w->rg.mute--; w->catch_depth--; }
            uint32_t out = mnemo_save(m);
            mnemo_restore(m, entry);
            mnemo_join(m, out);                        /* the try block may stop anywhere */
            rg_forget_assigned(&w->rg, n->b);          /* ... so what it assigns is unknown in the handlers */
            uint32_t into_catch = mnemo_save(m);
            for (int i = 0; i < n->l1.count; i++) {
                mnemo_restore(m, into_catch);
                stmt(w, n->l1.items[i]);
                mnemo_join_into(m, out);
            }
            if (n->c) w->fins.count--;                 /* the catches were still inside the try for a jump */
            mnemo_restore(m, out);
            if (n->c) {                                /* a finally also runs after an exception from anywhere in the try / catches */
                rg_forget_assigned(&w->rg, n->b);
                for (int i = 0; i < n->l1.count; i++) rg_forget_assigned(&w->rg, n->l1.items[i]);
                stmt(w, n->c);
            }
            mnemo_release(m, entry);
            mnemo_release(m, out);
            mnemo_release(m, into_catch);
            return;
        }
        case N_CATCH:
            if (n->sym) mnemo_reinit(m, id_of(n->sym));   /* a fresh variable on every iteration, like a local */
            stmt(w, n->b);
            return;
        case N_SWITCH: {
            expr(w, n->a);
            uint32_t entry = mnemo_save(m);
            Target t;
            push_target(w, &t, 1);
            uint32_t out = mnemo_save_dead(m);
            uint32_t fall = mnemo_save_dead(m);
            int has_default = 0;
            for (int i = 0; i < n->l1.count; i++) {
                Node *c = n->l1.items[i];
                if (c->flags == 1) has_default = 1;
                mnemo_restore(m, entry);
                rg_assume_case(&w->rg, n->a, c);       /* ... or the selector matched one of this case's labels */
                mnemo_join(m, fall);                   /* colon-form cases fall through */
                mnemo_release(m, fall);
                for (int j = 0; j < c->l1.count; j++) {   /* a type pattern binds a fresh variable */
                    Node *lb = c->l1.items[j];
                    if (lb->kind == N_TYPEPATTERN && lb->sym) mnemo_reinit(m, id_of(lb->sym));
                }
                if (c->b) {
                    if (c->b->kind == N_BLOCK || c->b->kind == N_THROW) stmt(w, c->b); else expr(w, c->b);
                    mnemo_join_into(m, out);
                    fall = mnemo_save_dead(m);
                } else {
                    stmts(w, &c->l2);
                    fall = mnemo_save(m);
                }
            }
            mnemo_restore(m, out);
            mnemo_join(m, fall);
            mnemo_join(m, t.brk);
            if (!has_default) mnemo_join(m, entry);
            mnemo_release(m, entry);
            mnemo_release(m, out);
            mnemo_release(m, fall);
            pop_target(w, &t);
            return;
        }
        default: return;
    }
}

/* ---------------------------------------------------------------- sole owners (what may be freed) */

/* A local is the sole owner of a fresh heap value, and gets freed at the end of its scope, when it is
 * declared directly in a block with a fresh String (a non-constant `+`) or array and is only ever
 * read in ways that cannot keep a second reference: indexing, `.length`, operands of binary operators,
 * String methods returning primitives, print arguments, the iterable of a for-each. Any other use
 * (passing it on, returning it, storing it, capturing it in a lambda, `T b = a`) leaves it to the
 * runtime, which never frees it: leaking is safe, freeing a shared value is not. Reassignments must
 * store another fresh value (or `+=`), so the old one can be freed first. */

static int const_expr(Node *e) {
    switch (e->kind) {
        case N_LIT_INT: case N_LIT_LONG: case N_LIT_FLOAT: case N_LIT_DOUBLE: case N_LIT_CHAR:
        case N_LIT_STRING: case N_LIT_BOOL: case N_LIT_NULL:
            return 1;
        case N_BINARY: return const_expr(e->a) && const_expr(e->b);
        case N_UNARY: return const_expr(e->a);
        default: return 0;
    }
}

/* ---- objects: `T o = new T(..)` where T is a plain class whose code never lets `this` escape ---- */

static int esc_walk(Node *n, JClass *cls);
static int this_escapes(JMethod *m, JClass *cls);

static int esc_list(JList *l, JClass *cls) {
    for (int i = 0; i < l->count; i++) if (esc_walk(l->items[i], cls)) return 1;
    return 0;
}

/* 1 when `this` of `cls` may be kept by the code in `n`: used as a value, captured, or handed to
 * code we cannot see. Field reads and writes through `this`, and calls to other non-escaping methods
 * of the same class, are fine. */
static int esc_walk(Node *n, JClass *cls) {
    if (!n) return 0;
    switch (n->kind) {
        case N_THIS: case N_SUPER: case N_LAMBDA: case N_METHODREF: return 1;
        case N_NAME: return n->rk == R_CAPTURED_THIS;
        case N_CTORCALL: return strcmp(n->name, "super") != 0 || n->l1.count != 0;   /* only `super()` of Object */
        case N_FIELDACC:
            return n->a && n->a->kind == N_THIS ? 0 : esc_walk(n->a, cls);
        case N_CALL: {
            JMethod *cm = n->sym;
            int on_this = !n->a || n->a->kind == N_THIS;
            if (on_this && cm && !(cm->flags & M_STATIC)) {
                if (cm->cls != cls || cm->tmpl || n->flags == 1 || this_escapes(cm, cls)) return 1;
            } else if (esc_walk(n->a, cls)) {
                return 1;
            }
            return esc_list(&n->l1, cls);
        }
        default: break;
    }
    if (esc_walk(n->a, cls) || esc_walk(n->b, cls) || esc_walk(n->c, cls)) return 1;
    return esc_list(&n->l1, cls) || esc_list(&n->l2, cls) || esc_list(&n->l3, cls) || esc_list(&n->l4, cls) || esc_list(&n->l5, cls);
}

static int this_escapes(JMethod *m, JClass *cls) {
    if (m->this_esc == 1) return 0;
    if (m->this_esc == 2 || m->this_esc == 3) return 1;   /* a cycle is treated as escaping */
    if (m->synthetic || !m->decl) return 0;
    m->this_esc = 3;
    int e = esc_walk(m->decl->b, cls);
    m->this_esc = e ? 2 : 1;
    return e;
}

/* an object nothing else can reference once `new` returns */
static int fresh_object(Node *e) {
    if (e->kind != N_NEW || !e->sym) return 0;
    JMethod *k = e->sym;
    JClass *c = k->cls;
    if (!c || !c->mem_on || k->tmpl || c->native_value || (c->kind != N_CLASS && c->kind != N_RECORD)) return 0;
    if (c->super && c->super->cls != jt_object()) return 0;
    if (c->decl && c->decl->l3.count) {                   /* interfaces: default methods are code we do not track */
        for (int i = 0; i < c->ifaces.count; i++) {
            JClass *ic = ((JType *)c->ifaces.items[i])->cls;
            for (int j = 0; ic && j < ic->methods.count; j++) if (((JMethod *)ic->methods.items[j])->flags & M_DEFAULT) return 0;
        }
    }
    if (this_escapes(k, c)) return 0;
    for (int i = 0; i < c->fields.count; i++) {           /* field initializers run inside the constructor */
        JField *f = c->fields.items[i];
        if (!(f->flags & M_STATIC) && f->decl && esc_walk(f->decl->b, c)) return 0;
    }
    return 1;
}

/* ash.util.List / ArrayList: represented by a plain array at run time */
static int list_type(JType *t) {
    return t && t->kind == TY_CLASS && t->cls && (!strcmp(t->cls->qname, "ash.util.ArrayList") || !strcmp(t->cls->qname, "ash.util.List"));
}

/* ash.util.Map / HashMap / Set / HashSet: an Ash map whose values are chains of [key, value, next] entries */
static int map_type(JType *t) {
    if (!t || t->kind != TY_CLASS || !t->cls) return 0;
    const char *q = t->cls->qname;
    return !strcmp(q, "ash.util.HashMap") || !strcmp(q, "ash.util.Map") || !strcmp(q, "ash.util.HashSet") || !strcmp(q, "ash.util.Set");
}

static int obj_type(JType *t) {
    return t && t->kind == TY_CLASS && t->cls && !ty_is_string(t) && !t->cls->native_value && !list_type(t) && !map_type(t);
}

/* `new HashMap<>()` / `new HashSet<>()` */
static int fresh_map(Node *e) {
    JMethod *k = e->kind == N_NEW ? e->sym : NULL;
    return k && k->tmpl && e->l1.count == 0 && k->cls &&
           (!strcmp(k->cls->qname, "ash.util.HashMap") || !strcmp(k->cls->qname, "ash.util.HashSet"));
}

/* map / set methods that neither return the container nor keep a reference to it (ash-stdlib HashMap.ash,
 * HashSet.ash: keys() / values() / toList() build new lists; iterator() walks such a list) */
static int map_method_ok(JMethod *m) {
    static const char *ok[] = {"size", "isEmpty", "put", "get", "containsKey", "remove", "keys", "values", "add", "contains", "toList", NULL};
    if (!m || !m->cls || !m->tmpl) return 0;
    for (int i = 0; ok[i]; i++) if (!strcmp(m->name, ok[i])) return 1;
    return 0;
}

/* `new ArrayList<>()`: an empty list nothing else references */
static int fresh_list(Node *e) {
    JMethod *k = e->kind == N_NEW ? e->sym : NULL;
    return k && k->tmpl && e->l1.count == 0 && k->cls && !strcmp(k->cls->qname, "ash.util.ArrayList");
}

/* methods of a list whose result and arguments cannot hand the list itself to anyone (templates in
 * ash-stdlib ArrayList.ash); sublists / streams / iterators copy or point back at it and are not here */
static int list_method_ok(JMethod *m) {
    static const char *ok[] = {"size", "isEmpty", "get", "set", "add", "remove", "removeLast", "contains", "sort", NULL};
    if (!m || !m->cls || !m->tmpl) return 0;
    for (int i = 0; ok[i]; i++) if (!strcmp(m->name, ok[i])) return !(!strcmp(m->name, "add") && m->params.count == 2);   /* add(int, T) may replace the list */
    return 0;
}

int jown_is_fresh_string(Node *e) {
    return e->kind == N_BINARY && !strcmp(e->name, "+") && e->type && ty_is_string(e->type) && !const_expr(e);
}

static int returns_fresh(JMethod *m);

/* a call to project code whose every return hands over a fresh String / array, so the caller becomes
 * its sole owner. The target must be fixed at compile time: no overriding is possible. */
static int fresh_call(Node *e) {
    if (e->kind != N_CALL || !e->sym) return 0;
    JMethod *m = e->sym;
    if (!m->cls || !m->cls->mem_on || m->tmpl || m->cls->kind == N_INTERFACE) return 0;
    if (!(m->flags & (M_STATIC | M_PRIVATE | M_FINAL)) && m->cls->subclasses.count) return 0;
    if (e->flags == 1) return 0;                                    /* super.m() */
    return returns_fresh(m);
}

static int is_fresh(Node *e) {
    return e->kind == N_NEWARRAY || e->kind == N_ARRAYINIT || jown_is_fresh_string(e) || fresh_object(e) || fresh_list(e) || fresh_map(e) || fresh_call(e);
}

static int is_cand(Node *n) {
    return n->kind == N_NAME && n->rk == R_LOCAL && n->sym && ((JLocal *)n->sym)->drop;
}

/* an instance field of a plain class, read by name or as `x.f` */
static int field_node(Node *n) {
    return (n->kind == N_NAME || n->kind == N_FIELDACC) && n->sym && (n->rk == R_FIELD || n->rk == R_THIS_FIELD);
}

/* a local that is still a sole-owner candidate, or a field that is still exclusively owned */
static int is_tracked(Node *n) {
    return is_cand(n) || (field_node(n) && ((JField *)n->sym)->own);
}

static JType *tracked_type(Node *n) {
    return field_node(n) ? ((JField *)n->sym)->type : ((JLocal *)n->sym)->type;
}

static void untrack(Node *n) {
    if (field_node(n)) ((JField *)n->sym)->own = 0;
    else if (n->sym) ((JLocal *)n->sym)->drop = 0;
}

/* ---- elements: a String[] / List<String> whose every stored element is a fresh String ---- */

static int str_elems(JType *t) {
    if (!t) return 0;
    if (t->kind == TY_ARRAY) return t->elem && ty_is_string(t->elem);
    return list_type(t) && t->nargs == 1 && t->args && ty_is_string(t->args[0]);
}

/* a T[][] (any depth): its elements are arrays; the container is freed together with the inner arrays only it holds */
static int arr_elems(JType *t) {
    return t && t->kind == TY_ARRAY && t->elem && t->elem->kind == TY_ARRAY;
}

static int elems_type(JType *t) { return str_elems(t) || arr_elems(t); }

/* an element that is a new heap value of the kind `t` holds (a fresh String, or a new array) and nothing else references yet */
static int elem_fresh(Node *e, JType *t) {
    if (arr_elems(t)) return e->kind == N_NEWARRAY || e->kind == N_ARRAYINIT;
    return jown_is_fresh_string(e);
}

/* the `elems` flag of a tracked container node, or NULL */
static int *elems_flag(Node *n) {
    if (!is_tracked(n)) return NULL;
    return field_node(n) ? &((JField *)n->sym)->elems : &((JLocal *)n->sym)->elems;
}

static void elems_off(Node *n) {
    int *f = elems_flag(n);
    if (f) *f = 0;
}

/* a container expression whose elements are known fresh Strings (or that has none yet) */
static int init_elems_fresh(Node *e, JType *t) {
    if (e->kind == N_NEWARRAY) {
        /* new String[n]: elements start as none. new T[a][b]: lowered so every inner array is its own new block
         * (__newarr2); three or more sizes are built by the generic helper, which kiln cannot free */
        if (!e->b) return !arr_elems(t) || e->l1.count <= 2;
        e = e->b;
    }
    if (e->kind == N_ARRAYINIT) {
        for (int i = 0; i < e->l1.count; i++) if (!elem_fresh(e->l1.items[i], t)) return 0;
        return 1;
    }
    return fresh_list(e);
}

static void alias_touch(Node *n) {                       /* a use of a for-each variable over an elems container */
    if (n->kind == N_NAME && n->rk == R_LOCAL && n->sym && ((JLocal *)n->sym)->elem_of) *((JLocal *)n->sym)->elem_of = 0;
}

static void collect(Node *n, int in_lambda) {
    if (!n) return;
    if (n->kind == N_LAMBDA) in_lambda = 1;
    if (n->kind == N_BLOCK && !in_lambda) {
        for (int i = 0; i < n->l1.count; i++) {
            Node *s = n->l1.items[i];
            JLocal *l = s->kind == N_LOCAL ? s->sym : NULL;
            if (l && s->b && is_fresh(s->b) &&
                (l->type->kind == TY_ARRAY || ty_is_string(l->type) || (list_type(l->type) && (fresh_list(s->b) || s->b->kind == N_CALL)) || (map_type(l->type) && fresh_map(s->b)) ||
                 (obj_type(l->type) && ((s->b->kind == N_NEW && l->type->cls == ((JMethod *)s->b->sym)->cls) ||
                                        (s->b->kind == N_CALL && ((JMethod *)s->b->sym)->fresh_cls == l->type->cls))))) { l->drop = 1; l->elems = elems_type(l->type) && init_elems_fresh(s->b, l->type); }
        }
    }
    collect(n->a, in_lambda); collect(n->b, in_lambda); collect(n->c, in_lambda);
    JList *ls[] = {&n->l1, &n->l2, &n->l3, &n->l4, &n->l5};
    for (int k = 0; k < 5; k++) for (int i = 0; i < ls[k]->count; i++) collect(ls[k]->items[i], in_lambda);
}

static const char *safe_methods[] = {"length", "charAt", "isEmpty", "equals", "equalsIgnoreCase", "startsWith",
                                     "endsWith", "contains", "indexOf", "lastIndexOf", "compareTo", "hashCode", NULL};

static int safe_string_method(JMethod *m) {
    if (!m || m->cls != jt_string()) return 0;
    for (int i = 0; safe_methods[i]; i++) if (!strcmp(m->name, safe_methods[i])) return 1;
    return 0;
}

static int print_method(JMethod *m) {
    return m && m->tmpl && (!strncmp(m->tmpl, "__println(", 10) || !strncmp(m->tmpl, "__print(", 8));
}

static void scan(Node *n, int in_lambda);
static int param_borrowed(JMethod *m, int i);

/* argument `i` goes to a parameter the callee only reads. A container whose elements the callee may take out loses its
 * `elems` status (the callee could keep an element, and freeing the inner values with the container would dangle) */
static void borrowed_arg(JMethod *m, int i, Node *arg, int in_lambda);

/* a use of `n` that cannot create a second reference */
static void fieldacc_base(Node *n, int in_lambda);

static int is_get_call(Node *n) {
    JMethod *m = n->kind == N_CALL ? n->sym : NULL;
    return m && n->a && m->tmpl && !strcmp(m->name, "get") && m->params.count == 1 && is_tracked(n->a) && list_type(tracked_type(n->a));
}

static void benign(Node *n, int in_lambda) {
    if (n->kind == N_INDEX && elems_flag(n->a)) {         /* xs[i] read where it cannot be kept */
        benign(n->a, in_lambda);
        scan(n->b, in_lambda);
        return;
    }
    if (is_get_call(n)) {                                  /* xs.get(i), likewise */
        benign(n->a, in_lambda);
        for (int i = 0; i < n->l1.count; i++) scan(n->l1.items[i], in_lambda);
        return;
    }
    if (n->kind == N_FIELDACC && field_node(n)) {       /* `x.f`: reading the field is fine, x is judged on its own */
        if (in_lambda && is_tracked(n)) untrack(n);
        fieldacc_base(n, in_lambda);
        return;
    }
    if (n->kind != N_NAME) { scan(n, in_lambda); return; }
    if (in_lambda) { if (is_tracked(n)) untrack(n); alias_touch(n); }   /* captured */
}

static void borrowed_arg(JMethod *m, int i, Node *arg, int in_lambda) {
    Node *t = is_borrow(arg) ? arg->a : arg;
    benign(t, in_lambda);
    Node *pn = m->decl->l1.items[i];
    JLocal *l = pn->kind == N_PARAM ? pn->sym : NULL;
    if (!l || !l->param_elems_ok) elems_off(t);
}

/* the target of `x.f` / `x.length` */
static void fieldacc_base(Node *n, int in_lambda) {
    if (is_cand(n->a) && obj_type(((JLocal *)n->a->sym)->type)) benign(n->a, in_lambda);
    else if (n->name && !strcmp(n->name, "length") && n->a->kind == N_NAME) benign(n->a, in_lambda);
    else if (n->name && !strcmp(n->name, "length") && n->a->kind == N_INDEX && elems_flag(n->a->a)) benign(n->a, in_lambda);   /* m[i].length */
    else scan(n->a, in_lambda);
}

static void scan(Node *n, int in_lambda) {
    if (!n) return;
    switch (n->kind) {
        case N_NAME:
            if (is_tracked(n)) untrack(n);                       /* any other use may alias */
            alias_touch(n);
            return;
        case N_INDEX:
            elems_off(n->a);                                     /* an element read outside the harmless contexts may keep it */
            benign(n->a, in_lambda);
            scan(n->b, in_lambda);
            return;
        case N_FIELDACC:
            if (is_tracked(n)) untrack(n);                       /* a field read outside the benign contexts may alias */
            fieldacc_base(n, in_lambda);
            return;
        case N_BINARY: {
            /* + on an object calls toString, which could keep it; identity tests cannot */
            int ident = !strcmp(n->name, "==") || !strcmp(n->name, "!=");
            Node *ops[2] = {n->a, n->b};
            for (int i = 0; i < 2; i++) {
                if (!ident && ops[i]->kind == N_NAME && is_cand(ops[i]) && obj_type(((JLocal *)ops[i]->sym)->type)) scan(ops[i], in_lambda);
                else benign(ops[i], in_lambda);
            }
            return;
        }
        case N_CALL: {
            JMethod *m = n->sym;
            if (n->a && n->a->kind == N_NAME && is_cand(n->a) && obj_type(((JLocal *)n->a->sym)->type)) {
                JClass *c = ((JLocal *)n->a->sym)->type->cls;
                JLocal *recv = n->a->sym;
                int fixed = !recv->is_param || (c->kind != N_INTERFACE && !c->subclasses.count);   /* a parameter may be a subclass */
                if (fixed && m && m->cls == c && !m->tmpl && !(m->flags & M_STATIC) && !this_escapes(m, c)) benign(n->a, in_lambda);
                else scan(n->a, in_lambda);
            } else if (n->a && is_tracked(n->a) && (list_type(tracked_type(n->a)) || map_type(tracked_type(n->a)))) {
                if (list_type(tracked_type(n->a)) ? list_method_ok(m) : map_method_ok(m)) benign(n->a, in_lambda); else scan(n->a, in_lambda);
                if (list_type(tracked_type(n->a)) && m && m->tmpl && elems_flag(n->a)) {
                    int last = n->l1.count - 1;
                    if (!strcmp(m->name, "get")) elems_off(n->a);                 /* reached here: not in a harmless context */
                    else if ((!strcmp(m->name, "add") && n->l1.count == 1) || (!strcmp(m->name, "set") && n->l1.count == 2)) {
                        if (!elem_fresh(n->l1.items[last], tracked_type(n->a)) || in_lambda) elems_off(n->a);   /* a shared / literal element */
                    }
                }
            } else if (n->a) { if (safe_string_method(m)) benign(n->a, in_lambda); else scan(n->a, in_lambda); }
            for (int i = 0; i < n->l1.count; i++) {
                Node *arg = n->l1.items[i];
                int obj_arg = arg->kind == N_NAME && is_cand(arg) && obj_type(((JLocal *)arg->sym)->type);
                if ((print_method(m) || safe_string_method(m)) && !obj_arg) benign(arg, in_lambda);
                else if (param_borrowed(m, i)) borrowed_arg(m, i, arg, in_lambda);
                else scan(n->l1.items[i], in_lambda);
            }
            return;
        }
        case N_EXPRSTMT:
            if (n->a->kind == N_ASSIGN && n->a->a->kind == N_INDEX && elems_flag(n->a->a->a)) {
                Node *as = n->a;                                 /* xs[i] = value */
                if (strcmp(as->name, "=") != 0 || !elem_fresh(as->b, tracked_type(as->a->a)) || in_lambda) elems_off(as->a->a);
                benign(as->a->a, in_lambda);
                scan(as->a->b, in_lambda);
                scan(as->b, in_lambda);
                return;
            }
            if (n->a->kind == N_ASSIGN && is_tracked(n->a->a)) {
                Node *as = n->a;
                if (elems_flag(as->a) && (strcmp(as->name, "=") != 0 || !init_elems_fresh(as->b, tracked_type(as->a)))) elems_off(as->a);   /* a new container with unknown elements */
                int ok = (!strcmp(as->name, "=") && is_fresh(as->b)) || (!strcmp(as->name, "+=") && ty_is_string(tracked_type(as->a)));
                if (!ok || in_lambda) untrack(as->a);
                if (as->a->kind == N_FIELDACC) fieldacc_base(as->a, in_lambda);
                if (!strcmp(as->name, "+=")) benign(as->b, in_lambda); else scan(as->b, in_lambda);
                return;
            }
            scan(n->a, in_lambda);
            return;
        case N_RETURN:
            /* a local's value goes to the caller (lowering skips freeing it on this path); a parameter's value
             * is the caller's own, so returning it is an escape */
            if (n->a && is_tracked(n->a) && (field_node(n->a) || ((JLocal *)n->a->sym)->is_param)) scan(n->a, in_lambda);
            else if (n->a) { if (n->a->kind == N_INDEX) elems_off(n->a->a); benign(n->a, in_lambda); }   /* `return xs[i]` hands out an element */
            return;
        case N_NEW: {
            JMethod *k = n->sym;
            for (int i = 0; i < n->l1.count; i++) {
                Node *arg = n->l1.items[i];
                if (param_borrowed(k, i)) borrowed_arg(k, i, arg, in_lambda);
                else scan(arg, in_lambda);
            }
            scan(n->a, in_lambda);
            return;
        }
        case N_FOREACH:
            if (n->sym && elems_flag(n->b)) ((JLocal *)n->sym)->elem_of = elems_flag(n->b);   /* the loop variable aliases the elements */
            benign(n->b, in_lambda);
            scan(n->c, in_lambda);
            return;
        case N_LAMBDA:
            scan(n->b, 1);
            return;
        default:
            break;
    }
    scan(n->a, in_lambda); scan(n->b, in_lambda); scan(n->c, in_lambda);
    JList *ls[] = {&n->l1, &n->l2, &n->l3, &n->l4, &n->l5};
    for (int k = 0; k < 5; k++) for (int i = 0; i < ls[k]->count; i++) scan(ls[k]->items[i], in_lambda);
}

/* ---------------------------------------------------------------- entry */

static int param_cand(JType *t) {
    return t && (t->kind == TY_ARRAY || ty_is_string(t) || obj_type(t) || list_type(t) || map_type(t));
}

static void analyze_method(JMethod *m) {
    if (m->own_done || !m->decl || !m->decl->b) return;
    m->own_done = 1;
    collect(m->decl->b, 0);
    /* parameters are analysed like sole-owner locals, with one difference: returning one hands the
     * caller's own value back, which is an escape */
    for (int i = 0; i < m->decl->l1.count; i++) {
        Node *pn = m->decl->l1.items[i];
        JLocal *l = pn->kind == N_PARAM ? pn->sym : NULL;
        if (l && param_cand(l->type) && !(m->flags & M_VARARGS)) { l->drop = 1; l->elems = elems_type(l->type); }
    }
    scan(m->decl->b, 0);
    for (int i = 0; i < m->decl->l1.count; i++) {
        Node *pn = m->decl->l1.items[i];
        JLocal *l = pn->kind == N_PARAM ? pn->sym : NULL;
        if (l) { l->param_ok = l->drop; l->param_elems_ok = l->drop && l->elems; l->drop = 0; l->elems = 0; }   /* a parameter is never freed by the callee */
    }
}

/* project code whose body we can see and whose target is fixed at compile time (no override can run instead) */
static int fixed_project_target(JMethod *m) {
    if (!m || m->tmpl || !m->cls || !m->cls->mem_on || !m->decl || !m->decl->b || (m->flags & M_VARARGS)) return 0;
    if (!m->is_ctor && !(m->flags & (M_STATIC | M_PRIVATE | M_FINAL)) && (m->cls->kind == N_INTERFACE || m->cls->subclasses.count)) return 0;
    return 1;
}

/* 1 when argument `i` of a call to `m` is only read by the callee, so the caller keeps owning what it passes
 * (and nothing is moved). The target must be fixed at compile time (no override can run instead). */
static int param_borrowed(JMethod *m, int i) {
    if (!m || m->tmpl || !m->cls || !m->cls->mem_on || !m->decl || !m->decl->b || (m->flags & M_VARARGS)) return 0;
    if (!m->is_ctor && !(m->flags & (M_STATIC | M_PRIVATE | M_FINAL)) && (m->cls->kind == N_INTERFACE || m->cls->subclasses.count)) return 0;
    if (i >= m->decl->l1.count) return 0;
    analyze_method(m);
    Node *pn = m->decl->l1.items[i];
    JLocal *l = pn->kind == N_PARAM ? pn->sym : NULL;
    return l && l->param_ok;
}

/* every `return e` outside lambdas hands over a fresh value, or a local that is its sole owner; for
 * objects they must all be the same exact class so the caller knows what it owns */
static int all_returns_fresh(Node *n, int *seen, JClass **cls, int *cls_set) {
    if (!n || n->kind == N_LAMBDA) return 1;
    if (n->kind == N_RETURN) {
        (*seen)++;
        Node *e = n->a;
        if (!e) return 0;
        JClass *c = NULL;
        if (fresh_object(e)) c = ((JMethod *)e->sym)->cls;
        else if (e->kind == N_NAME && is_cand(e)) { JType *t = ((JLocal *)e->sym)->type; c = obj_type(t) ? t->cls : NULL; }
        else if (!is_fresh(e)) return 0;
        if (*cls_set && *cls != c) return 0;
        *cls = c;
        *cls_set = 1;
        return 1;
    }
    if (!all_returns_fresh(n->a, seen, cls, cls_set) || !all_returns_fresh(n->b, seen, cls, cls_set) ||
        !all_returns_fresh(n->c, seen, cls, cls_set)) return 0;
    JList *ls[] = {&n->l1, &n->l2, &n->l3, &n->l4, &n->l5};
    for (int k = 0; k < 5; k++) for (int i = 0; i < ls[k]->count; i++) if (!all_returns_fresh(ls[k]->items[i], seen, cls, cls_set)) return 0;
    return 1;
}

static int returns_fresh(JMethod *m) {
    if (m->fresh_ret == 1) return 1;
    if (m->fresh_ret >= 2) return 0;                  /* no, or a recursive cycle */
    if (!m->decl || !m->decl->b || m->is_ctor || !m->ret || !(m->ret->kind == TY_ARRAY || ty_is_string(m->ret) || list_type(m->ret) || obj_type(m->ret))) return 0;
    m->fresh_ret = 3;
    analyze_method(m);
    int seen = 0, cls_set = 0;
    JClass *cls = NULL;
    int ok = all_returns_fresh(m->decl->b, &seen, &cls, &cls_set) && seen > 0;
    if (ok && obj_type(m->ret) && (!cls || cls != m->ret->cls)) ok = 0;      /* an object must be exactly the declared class */
    if (ok && !obj_type(m->ret) && cls) ok = 0;
    m->fresh_cls = ok ? cls : NULL;
    m->fresh_ret = ok ? 1 : 2;
    return ok;
}

static void check_method(JMethod *m, int flow) {
    if (!m->decl || !m->decl->b) return;
    analyze_method(m);
    if (!flow) return;
    rg_check_recursion(m);
    Walk w = {0};
    w.m = mnemo_walk_new();
    w.rg.m = w.m;
    rg_prescan(m->decl->b);
    region_mark_loops(m->decl->b);
    stmt(&w, m->decl->b);
    mnemo_walk_free(w.m);
}

static int under(const char *path, const char *root) {
    if (path && !strcmp(root, ".")) return !strchr(path, '/');   /* entry file in the working directory */
    size_t n = strlen(root);
    return path && strncmp(path, root, n) == 0 && (path[n] == '/' || path[n] == 0);
}

int jown_owned_fields(JClass *c, JField ***out) {
    static JField *buf[64];
    int n = 0;
    for (int k = 0; k < c->fields.count && n < 64; k++) {
        JField *f = c->fields.items[k];
        if (f->own && !(f->flags & M_STATIC)) buf[n++] = f;
    }
    *out = buf;
    return n;
}

int jown_local_elems(JLocal *l) { return l->drop && l->elems; }
int jown_field_elems(JField *f) { return f->own && f->elems; }

int jown_field_assign_frees(Node *n) {
    if (!field_node(n) || !((JField *)n->sym)->own) return 0;
    if (n->kind == N_NAME) return 1;
    return n->a && (n->a->kind == N_THIS || (n->a->kind == N_NAME && n->a->rk == R_LOCAL));   /* no side effects when evaluated twice */
}

void jown_check_project(const char *entry_path) {
    char *root = find_project_root(entry_path);
    if (!root || !memory_safety_on(root)) return;
    JClass **all;
    int n = jt_all_classes(&all);
    for (int i = 0; i < n; i++) if (under(all[i]->path, root)) all[i]->mem_on = 1;   /* before any analysis reads it */
    /* fields start as "exclusively owned" candidates (a plain class's String / array instance fields) and lose
     * the status at the first use that could alias or replace them with a shared value */
    for (int i = 0; i < n; i++) {
        JClass *c = all[i];
        if (!c->mem_on || c->kind != N_CLASS) continue;
        for (int k = 0; k < c->fields.count; k++) {
            JField *f = c->fields.items[k];
            f->own = !(f->flags & M_STATIC) && f->type && (f->type->kind == TY_ARRAY || ty_is_string(f->type) || list_type(f->type) || map_type(f->type));
            f->elems = f->own && elems_type(f->type) && !(f->decl && f->decl->b);   /* no initializer: elements start empty */
        }
    }
    for (int i = 0; i < n; i++) {
        JClass *c = all[i];
        if (!c->mem_on || c->kind != N_CLASS) continue;
        for (int k = 0; k < c->fields.count; k++) {
            JField *f = c->fields.items[k];
            if (!f->own || !f->decl || !f->decl->b) continue;
            if (!is_fresh(f->decl->b)) f->own = 0;               /* a literal / alias initializer: the value is shared */
            f->elems = f->own && elems_type(f->type) && init_elems_fresh(f->decl->b, f->type);
            scan(f->decl->b, 0);
        }
    }
    rg_prepare_project(all, n);
    for (int i = 0; i < n; i++) {
        JClass *c = all[i];
        if (!c->mem_on) continue;
        for (int k = 0; k < c->methods.count; k++) check_method(c->methods.items[k], 1);
        for (int k = 0; k < c->ctors.count; k++) check_method(c->ctors.items[k], 1);
    }
}
