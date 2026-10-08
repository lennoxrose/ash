#ifndef PYRE_MANAGER_H
#define PYRE_MANAGER_H
// The manager: Pyre's resident decision-maker. For every kernel call it
// looks up what this machine has actually been seen to do at this workload
// size (cost_model.h), runs the work on the fastest target, times it, and
// records the result -- so it keeps learning from the program's own work
// with no calibration step and no added delay:
//
//  * A call NEVER waits for a GPU to start. Until one is up the CPU runs the
//    work; when the work is heavy, a background thread brings the GPU up
//    meanwhile (targets.h: pyre_target_warm_async).
//  * A GPU is only chosen where it was measured -- or can be inferred from a
//    smaller measured size -- to beat the CPU. A size it has never run at
//    gets one real try, and only when the job is big enough
//    (EXPLORE_MIN_S) that one slow try is cheap.
//  * What it learns is saved for the next run on the same machine, as an
//    optimisation only (it is never shipped, never required).
#include "H/runtime/cost_model.h"

typedef int (*PyreRunFn)(PyreTarget t, void *args);  // 1 = ran, 0 = could not

// How much each kernel call costs in bytes moved and work done.
PyreWorkload pyre_workload_matmul(int n);
PyreWorkload pyre_workload_chaos(int n, int iterations);
PyreWorkload pyre_workload_monte_carlo(long long iterations);

// The profile in force (loaded on first use; empty if none). Never NULL.
const PyreProfile *pyre_manager_profile(void);
const char *pyre_manager_profile_path(void);  // "" if persistence is off

// Developer tooling (tools/pyre.c): edit the profile directly, persist it,
// or throw it away.
PyreProfile *pyre_manager_profile_edit(void);
int pyre_manager_save(void);
void pyre_manager_forget(void);

// The "should I take this over?" question without doing the work: predicted
// seconds per target (< 0 = not known / can't run it), returning the target
// the manager would use right now.
PyreTarget pyre_manager_estimate(PyreOp op, PyreWorkload w, double seconds[PYRE_TARGET_COUNT]);

// Runs the workload on the best target; a target that fails to start or to
// run is taken out of rotation and the work falls through, ending at the
// CPU, which always runs.
void pyre_manager_execute(PyreOp op, PyreWorkload w, PyreRunFn run, void *args);

#endif
