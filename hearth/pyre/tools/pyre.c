/* `pyre`: developer tool for the manager (src/C/runtime/manager.c).
 *
 * The runtime that ships inside programs never calibrates and never delays
 * a call; it learns this machine from the work the program really does.
 * This tool is for the developer who wants to look at, or pre-teach, that
 * learning on THIS machine. Nothing it produces is shipped: it only edits
 * the local profile (~/.cache/pyre/profile.txt, or $PYRE_PROFILE).
 *
 *   pyre info                        devices, profile file, everything learned
 *   pyre plan matmul N               what the manager would do for this job
 *   pyre plan chaos N ITERATIONS
 *   pyre plan mc ITERATIONS
 *   pyre train [flags]               run real probes on this machine and teach it
 *   pyre forget                      delete what was learned (relearned on demand)
 *
 * `pyre help` and `pyre help train` list every flag.
 *
 * Set PYRE_TRACE=1 on any program to watch the manager's live decisions.
 */
#define _POSIX_C_SOURCE 200809L
#include "H/runtime/ash_gpu.h"
#include "H/runtime/manager.h"
#include "dashboard.h"
#include "json_out.h"
#include "train.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int json_mode;  /* --json: machine-readable output (json_out.h) */

/* ---------- info / plan ---------- */

static void print_rates(const PyreProfile *p) {
    static const char *unit[PYRE_OP_COUNT] = {"mul-add/s", "elem-iter/s", "iter/s"};
    int any = 0;
    for (int o = 0; o < PYRE_OP_COUNT; o++) {
        for (int t = 0; t < PYRE_TARGET_COUNT; t++) {
            const PyreRates *r = &p->rates[o][t];
            int n = 0;
            for (int b = 0; b < PYRE_BUCKETS; b++) n += r->seen[b];
            if (!n) continue;
            if (!any) { printf("\nlearned (effective speed by workload size, 2^k work units):\n"); any = 1; }
            printf("  %-17s %-7s", pyre_op_name(o), pyre_target_name(t));
            for (int b = 0; b < PYRE_BUCKETS; b++) {
                if (r->seen[b]) printf("  2^%d:%.2g", b, 1.0 / r->rate[b]);
            }
            printf("  %s\n", unit[o]);
        }
    }
    if (!any) printf("\nnothing learned yet (programs learn as they run; or `pyre train`)\n");
}

static void cmd_info_json(void) {
    const PyreProfile *p = pyre_manager_profile();
    jbegin("info");
    jkey("profile"); jstr(pyre_manager_profile_path());
    jkey("cpu_threads"); printf("%d", p->cpu_threads);
    jkey("cuda_built"); printf("%d", p->cuda_built);
    jkey("targets"); printf("[");
    for (int t = 0; t < PYRE_TARGET_COUNT; t++) {
        printf("%s{\"name\":", t ? "," : ""); jstr(pyre_target_name(t));
        printf(",\"present\":%d,\"device\":", t == PYRE_CPU ? 1 : p->present[t]);
        jstr(t == PYRE_CPU ? "CPU" : p->device[t]);
        printf(",\"init_ms\":%.3f}", p->init_seconds[t] * 1e3);
    }
    printf("]");
    jkey("rates"); printf("[");
    int first = 1;
    for (int o = 0; o < PYRE_OP_COUNT; o++) {
        for (int t = 0; t < PYRE_TARGET_COUNT; t++) {
            for (int b = 0; b < PYRE_BUCKETS; b++) {
                if (!p->rates[o][t].seen[b]) continue;
                printf("%s{\"op\":", first ? "" : ","); jstr(pyre_op_name(o));
                printf(",\"target\":"); jstr(pyre_target_name(t));
                printf(",\"bucket\":%d,\"per_second\":%.6g}", b, 1.0 / p->rates[o][t].rate[b]);
                first = 0;
            }
        }
    }
    printf("]");
    jend();
}

static void cmd_info(void) {
    if (json_mode) { cmd_info_json(); return; }
    const PyreProfile *p = pyre_manager_profile();
    printf("profile: %s\n\ntargets:\n", pyre_manager_profile_path()[0] ? pyre_manager_profile_path() : "(memory only)");
    printf("  %-7s CPU (%d threads)\n", "cpu", p->cpu_threads);
    for (int t = PYRE_CUDA; t < PYRE_TARGET_COUNT; t++) {
        if (p->present[t]) {
            printf("  %-7s %s", pyre_target_name(t), p->device[t]);
            if (p->init_seconds[t] > 0) printf("   (last start-up %.0f ms)", p->init_seconds[t] * 1e3);
            printf("\n");
        } else {
            printf("  %-7s not seen yet%s\n", pyre_target_name(t),
                   t == PYRE_CUDA && !p->cuda_built ? " (built without CUDA)" : "");
        }
    }
    print_rates(p);
}

static void cmd_plan(PyreOp op, PyreWorkload w, const char *what) {
    double est[PYRE_TARGET_COUNT];
    PyreTarget best = pyre_manager_estimate(op, w, est);
    printf("%s\n", what);
    for (int t = 0; t < PYRE_TARGET_COUNT; t++) {
        if (est[t] < 0) printf("  %-7s not measured at this size\n", pyre_target_name(t));
        else printf("  %-7s %10.4f ms%s\n", pyre_target_name(t), est[t] * 1e3, t == (int)best ? "   <-- would run here" : "");
    }
}

/* ---------- help ---------- */

static void help_main(void) {
    puts("pyre - look at, and teach, Pyre's placement manager on THIS machine\n"
         "\n"
         "The runtime inside your programs never calibrates and never delays a call;\n"
         "it learns the machine from the work the program really does. This tool is\n"
         "for developers: inspect what was learned, ask what the manager would do,\n"
         "or pre-teach it with real probes. Nothing here is shipped with programs.\n"
         "\n"
         "usage: pyre <command> [options]\n"
         "\n"
         "commands:\n"
         "  info                          devices, profile file, everything learned so far\n"
         "  plan matmul N                 what the manager would do for an N x N multiply\n"
         "  plan chaos N ITERATIONS       ... for chaos_iterate\n"
         "  plan mc ITERATIONS            ... for monte_carlo_risk\n"
         "  train [options]               run real probes and teach this machine's profile;\n"
         "                                checks itself before and after, and undoes training\n"
         "                                that made decisions worse\n"
         "  check                         score the manager's decisions right now: runs every\n"
         "                                target on held-out sizes and compares its picks\n"
         "  dashboard [--port N] [--no-open]\n"
         "                                (always http://127.0.0.1:1198 unless --port; local only)\n"
         "                                local, offline web dashboard: analyse what was learned,\n"
         "                                start / watch / stop training sessions, see history\n"
         "                                (needs `make web` to have built the UI)\n"
         "  forget                        delete what was learned (relearned on demand)\n"
         "  help [command]                this text, or details for one command\n"
         "\n"
         "environment:\n"
         "  PYRE_TRACE=1      any program prints one line per kernel call: where it ran,\n"
         "                    what each target was predicted to take, how long it took\n"
         "  PYRE_PROFILE=F    use F as the profile file instead of ~/.cache/pyre/profile.txt");
}

static void help_train(void) {
    puts("pyre train - run real probes on this machine and save what they show\n"
         "\n"
         "usage: pyre train [options]\n"
         "\n"
         "Each probe runs a real kernel at a size of 2^k work units, from 2^12 up,\n"
         "on every chosen target, and records how fast it was. It stops growing a\n"
         "target's size once a probe gets slow, so a weak device stays quick.\n"
         "\n"
         "adaptive learning (the way a model learns, not a fixed grid):\n"
         "  --auto            grade the manager on fresh held-out sizes, find where its\n"
         "                    decisions are weakest, practise exactly there, grade again, keep\n"
         "                    the round only if decisions did not get worse; practises less each\n"
         "                    round and stops by itself once it has converged\n"
         "  --rounds N        most rounds --auto may run (1..1000, default 6)\n"
         "  --forever         the same loop with no end: keeps re-checking and re-practising\n"
         "                    (so it also notices drift) until you stop it (Ctrl+C, or Stop in\n"
         "                    the dashboard); saves after every kept round\n"
         "  --round-pause S   seconds to idle between --forever rounds (default 20)\n"
         "\n"
         "how thorough:\n"
         "  --level quick|normal|deep   preset for step, repeats and --max-probe\n"
         "                              quick:  every 3rd size, 1 run,  0.25 s probes\n"
         "                              normal: every 2nd size, 2 runs, 1 s probes (default)\n"
         "                              deep:   every size,     3 runs, 3 s probes\n"
         "  --step N          sizes to skip between probes: 1 = every power of two (1..12)\n"
         "  --repeats N       runs per size; more smooths noise (1..50)\n"
         "  --max-work EXP    largest size, as 2^EXP work units (12..40, default 34)\n"
         "  --max-probe SECS  stop growing once one probe takes longer than this\n"
         "  --max-mem MB      do not run probes that would move more than this many MB\n"
         "\n"
         "how much of the machine it may use (the load scale):\n"
         "  --gpu-load PCT    1..100 (default 100). Duty cycle on GPU targets: after each\n"
         "                    probe it idles so the GPU is busy about PCT% of the time\n"
         "  --cpu-load PCT    1..100 (default 100). Duty cycle on the CPU target AND the\n"
         "                    share of CPU threads probes may use\n"
         "  --pause MS        extra idle after every probe, on any target\n"
         "                    (the load scale limits the AVERAGE load; to shrink single\n"
         "                    probes use --max-probe, --max-work or --max-mem)\n"
         "\n"
         "what to train:\n"
         "  --ops LIST        matmul,chaos,mc          (default: all)\n"
         "  --targets LIST    cpu,cuda,vulkan          (default: all that start)\n"
         "  --reset           forget everything learned before training\n"
         "  --quiet           print only the summary\n"
         "  --no-verify       skip the before/after self-check (faster, no safety net)\n"
         "\n"
         "self-check: unless --no-verify, training is graded on sizes it did not\n"
         "train on. Every target really runs them; the manager's pick is scored by\n"
         "how much slower it was than the truly fastest target, and by how far its\n"
         "predicted time was from the real one. If decisions got worse, the training\n"
         "is thrown away and the previous knowledge kept.\n"
         "\n"
         "examples:\n"
         "  pyre train --auto                          adaptive: practise where it is weakest\n"
         "  pyre train --forever --gpu-load 25         keep learning in the background, gently\n"
         "  pyre train --level quick                   fast first pass\n"
         "  pyre train --level deep --targets cuda     thorough, CUDA only\n"
         "  pyre train --gpu-load 25 --pause 50        gentle: GPU about a quarter busy\n"
         "  pyre train --ops chaos --max-work 28 --max-mem 256 --reset");
}

int main(int argc, char **argv) {
    /* --json anywhere: machine-readable one-object-per-line output (the dashboard's feed). */
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--json")) {
            json_mode = 1;
            for (int j = i; j < argc; j++) argv[j] = argv[j + 1];
            argc--;
            i--;
        }
    }
    if (json_mode) setvbuf(stdout, NULL, _IOLBF, 0);
    if (argc < 2 || !strcmp(argv[1], "help") || !strcmp(argv[1], "--help") || !strcmp(argv[1], "-h")) {
        if (argc >= 3 && !strcmp(argv[2], "train")) help_train(); else help_main();
        return 0;
    }
    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
            if (!strcmp(argv[1], "train")) help_train(); else help_main();
            return 0;
        }
    }
    if (!strcmp(argv[1], "dashboard")) return pyre_dashboard_main(argc, argv);
    if (!strcmp(argv[1], "info")) { cmd_info(); return 0; }
    if (!strcmp(argv[1], "check")) return cmd_check();
    if (!strcmp(argv[1], "train")) return cmd_train(argc, argv);
    if (!strcmp(argv[1], "forget")) {
        pyre_manager_forget();
        if (json_mode) { jbegin("done"); jkey("probes"); printf("0"); jkey("seconds"); printf("0"); jkey("saved"); printf("true"); jend(); }
        else printf("forgot; relearned on demand\n");
        return 0;
    }
    if (argc == 4 && !strcmp(argv[1], "plan") && !strcmp(argv[2], "matmul")) {
        int n = atoi(argv[3]);
        char what[64];
        snprintf(what, sizeof(what), "matrix_multiply %dx%d:", n, n);
        cmd_plan(PYRE_OP_MATMUL, pyre_workload_matmul(n), what);
        return 0;
    }
    if (argc == 5 && !strcmp(argv[1], "plan") && !strcmp(argv[2], "chaos")) {
        int n = atoi(argv[3]), it = atoi(argv[4]);
        char what[80];
        snprintf(what, sizeof(what), "chaos_iterate n=%d iterations=%d:", n, it);
        cmd_plan(PYRE_OP_CHAOS, pyre_workload_chaos(n, it), what);
        return 0;
    }
    if (argc == 4 && !strcmp(argv[1], "plan") && !strcmp(argv[2], "mc")) {
        long long it = atoll(argv[3]);
        char what[64];
        snprintf(what, sizeof(what), "monte_carlo_risk iterations=%lld:", it);
        cmd_plan(PYRE_OP_MONTE_CARLO, pyre_workload_monte_carlo(it), what);
        return 0;
    }
    fprintf(stderr, "pyre: unknown command or arguments '%s' -- run `pyre help`\n", argv[1]);
    return 2;
}
