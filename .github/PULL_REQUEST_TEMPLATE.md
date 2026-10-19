## What and why

<!-- One paragraph. Link the issue: Fixes #123 -->

## Performance (required)

Ash must never get slower. Paste the output of `tools/bench_compare.sh /tmp/ashvm_base bin/ashvm`
(see CONTRIBUTING.md). For `kiln` code-generation changes, add before/after timings of compiled programs.
If the change cannot affect speed (docs, tests, tooling), say so.

```
<output here>
```

## Checklist

- [ ] No benchmark is slower than `main` (output above ends with `OK`)
- [ ] `make` builds warning-free
- [ ] Relevant suites pass with unchanged output: ashvm / kiln / front / mnemo / apm
- [ ] New test in `hearth/<project>/tests/cases/<feature>/` (feature or bug fix)
- [ ] `CHANGELOG.md` entry added at the top
- [ ] `README.md` updated if user-visible behavior changed
- [ ] Follows the layout rules in `AGENTS.md` (mirrored `C/ H/ D/ O/`, small directories, no new dependencies)
