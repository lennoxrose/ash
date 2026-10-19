# Contributing to Ash

Thanks for helping. Ash is a from-scratch language in C with no external dependencies, and it stays fast on
purpose. Read this once; it is short.

## The law: nothing gets slower

**A change must never make Ash slower. A speed-up is welcome; a slowdown is rejected, whatever else it
brings.** This covers the VM (`hearth/ashvm/src/vm`), the compiler's fused ops, the front end, and the code
`kiln` generates. A feature that costs run-time speed must be restructured until it costs none (do the work
at compile time, behind a flag that is off by default, or in a path that only runs when the feature is used).

Prove it, do not claim it. Before you open a PR:

```sh
# 1. baseline: build main and keep the binary
git checkout main && make @ashvm && cp bin/ashvm /tmp/ashvm_base
# 2. your branch
git checkout - && make @ashvm
# 3. compare every benchmark in hearth/ashvm/benchmarks (best of 7 alternating runs)
tools/bench_compare.sh /tmp/ashvm_base bin/ashvm
```

It exits non-zero if any benchmark is more than 2% slower (the measured noise of identical code relinked in
a different order) or if the new binary fails on one. Paste its output into the PR. Changes to `kiln` code
generation also need before/after timings of the compiled programs. Reference points: `fib(30)` about 0.06 s,
sum to 20M about 0.12 s.

## Setup

```sh
tools/install.sh        # toolchain: gcc, make, Rust (mnemo), mingw, ...; --check only reports
make                    # clean build of everything into bin/
make @ashvm             # clean build of one project (also @kiln, @apm, ...)
```

The tests expect a checkout of [`ash-stdlib`](https://github.com/lennoxrose/ash-stdlib) next to this repo
(or `ASH_PATH` pointing at its `src/main/ash`).

## Tests

Run the suites for what you touched, and `make` warning-free (`-Wall -Wextra -std=c11 -O2`):

```sh
hearth/ashvm/tests/scripts/run_regression.sh
hearth/kiln/tests/scripts/run_regression.sh
hearth/front/tests/scripts/check_types.sh
hearth/mnemo/tests/scripts/run_regression.sh
hearth/apm/tests/scripts/smoke_test.sh
```

Output must be byte-identical to the `.out` files unless the change is intentional. A new feature or a bug
fix comes with a test: `hearth/<project>/tests/cases/<feature>/Name.ash` plus its `.out`. A bug fix starts
with a test that fails.

## Code rules

- Follow the layout in `AGENTS.md`: mirrored `C/ H/ D/ O/` trees, about 20 files per directory, features in
  folders, other languages only behind a C ABI header. Reuse an existing module before adding one.
- Match the surrounding code: naming, comment density, style. No new dependencies.
- Keep structural moves separate from behavior changes, so regression diffs stay meaningful.
- Add a `CHANGELOG.md` entry at the **top**, headed `## YYYY-MM-DD (topic)`, describing what really changed.
- Update `README.md` when user-visible behavior or layout changes.

## Pull requests

1. Open an issue first for anything big; small fixes can go straight to a PR.
2. One topic per PR, on a branch from `main`. Commit messages look like `feat(kiln): ...`, `fix(front): ...`.
3. Fill in the PR template, including the benchmark output.
4. A maintainer reviews. Correctness, memory safety and speed are the review criteria, in that order of
   "cannot be compromised".

By contributing you agree that your contribution is licensed under the [MIT License](../LICENSE).
Everyone taking part follows the [Code of Conduct](CODE_OF_CONDUCT.md).
