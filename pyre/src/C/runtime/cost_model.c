#define _POSIX_C_SOURCE 200809L
#include "H/runtime/cost_model.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define OBSERVE_RATE 0.3

int pyre_work_bucket(double work) {
    if (work < 1.0) return 0;
    int b = (int)floor(log2(work));
    return b >= PYRE_BUCKETS ? PYRE_BUCKETS - 1 : b;
}

void pyre_rates_observe(PyreRates *r, double work, double seconds) {
    if (work <= 0.0 || seconds <= 0.0) return;
    int b = pyre_work_bucket(work);
    double rate = seconds / work;
    if (!r->seen[b]) { r->rate[b] = rate; r->seen[b] = 1; }
    else r->rate[b] += (rate - r->rate[b]) * OBSERVE_RATE;
}

double pyre_rates_known(const PyreRates *r, double work) {
    int b = pyre_work_bucket(work);
    return r->seen[b] ? r->rate[b] * work : -1.0;
}

double pyre_rates_from_below(const PyreRates *r, double work) {
    for (int b = pyre_work_bucket(work); b >= 0; b--) {
        if (r->seen[b]) return r->rate[b] * work;
    }
    return -1.0;
}

double pyre_rates_nearest(const PyreRates *r, double work) {
    int home = pyre_work_bucket(work);
    for (int d = 0; d < PYRE_BUCKETS; d++) {
        if (home - d >= 0 && r->seen[home - d]) return r->rate[home - d] * work;
        if (home + d < PYRE_BUCKETS && r->seen[home + d]) return r->rate[home + d] * work;
    }
    return -1.0;
}

int pyre_profile_load(PyreProfile *p, const char *path) {
    memset(p, 0, sizeof(*p));
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    int version = 0;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char word[32], opn[32], name[128];
        int present, bucket;
        double v;
        if (sscanf(line, "version %d", &version) == 1) continue;
        if (sscanf(line, "cpu_threads %d", &p->cpu_threads) == 1) continue;
        if (sscanf(line, "cuda_built %d", &p->cuda_built) == 1) continue;
        name[0] = '\0';
        // "target <name> <present> <init_seconds> <device...>"
        int got = sscanf(line, "target %31s %d %lf %127[^\n]", word, &present, &v, name);
        if (got >= 3) {
            for (int t = 0; t < PYRE_TARGET_COUNT; t++) {
                if (strcmp(word, pyre_target_name(t))) continue;
                p->present[t] = present;
                p->init_seconds[t] = v;
                snprintf(p->device[t], sizeof(p->device[t]), "%s", got == 4 ? name : "");
            }
            continue;
        }
        // "rate <op> <target> <bucket> <seconds_per_work>"
        if (sscanf(line, "rate %31s %31s %d %lf", opn, word, &bucket, &v) == 4 && bucket >= 0 && bucket < PYRE_BUCKETS) {
            for (int o = 0; o < PYRE_OP_COUNT; o++) {
                if (strcmp(opn, pyre_op_name(o))) continue;
                for (int t = 0; t < PYRE_TARGET_COUNT; t++) {
                    if (strcmp(word, pyre_target_name(t))) continue;
                    p->rates[o][t].rate[bucket] = v;
                    p->rates[o][t].seen[bucket] = 1;
                }
            }
        }
    }
    fclose(f);
    return version == PYRE_PROFILE_VERSION;
}

int pyre_profile_save(const PyreProfile *p, const char *path) {
    char tmp[1100];
    snprintf(tmp, sizeof(tmp), "%s.%d.tmp", path, (int)getpid());
    FILE *f = fopen(tmp, "w");
    if (!f) return 0;
    fprintf(f, "# Pyre: what this machine was seen to do, learned from real calls.\n"
               "# Local to this machine, safe to delete (it is simply relearned).\n");
    fprintf(f, "version %d\ncpu_threads %d\ncuda_built %d\n", PYRE_PROFILE_VERSION, p->cpu_threads, p->cuda_built);
    for (int t = 0; t < PYRE_TARGET_COUNT; t++) {
        fprintf(f, "target %s %d %.9g %s\n", pyre_target_name(t), p->present[t], p->init_seconds[t], p->device[t]);
    }
    for (int o = 0; o < PYRE_OP_COUNT; o++) {
        for (int t = 0; t < PYRE_TARGET_COUNT; t++) {
            for (int b = 0; b < PYRE_BUCKETS; b++) {
                if (p->rates[o][t].seen[b]) {
                    fprintf(f, "rate %s %s %d %.9g\n", pyre_op_name(o), pyre_target_name(t), b, p->rates[o][t].rate[b]);
                }
            }
        }
    }
    int ok = fclose(f) == 0;
    if (ok && rename(tmp, path) != 0) ok = 0;
    if (!ok) remove(tmp);
    return ok;
}
