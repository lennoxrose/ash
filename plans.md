# Ash: Plans

Status: written 2026-10-07, right after the source/test restructure (see `CHANGELOG.md`).
Items marked **(proposed)** are my recommendations, not decisions. Items marked **(open)** still need an answer.

## Why

Ash is in a good position today: a working lexer, single-pass compiler and bytecode VM with closures, maps, `try`/`catch`, real doubles, imports and good diagnostics. But I don't like the general design anymore. Ash currently looks like C/JavaScript: braces, semicolons, parentheses around every condition. I love Lua, and I want Ash to read like Lua: lightweight, high level, little punctuation noise, with blocks that read as structure.

The goal is **a higher-level, Lua-looking language with the same performance**. The look changes. The engine's speed does not.

## Goals

1. **Lua-like surface syntax.** Blocks are keyword-delimited instead of brace-delimited, and the syntax carries less punctuation overall.
2. **Same performance.** The VM must not get slower. See the performance rules below. This is the hard constraint.
3. **Same behavior.** Existing language semantics stay unless a change is deliberate and written down.
4. **Keep the code organized.** Every change follows `AGENTS.md` (mirrored C/H/D/O trees, small feature folders, tests in `tests/cases/<feature>/`).

## Non-goals (for this phase)

- No new VM instructions or new runtime features just because the syntax changed. The syntax change is a **frontend** change.
- No type system, no classes/structs, no new standard library. If "high level" later means more language features (classes, varargs, default parameters), that is a separate plan.
- No JIT or backend work (the x86-64 JIT experiment in `ashvm/benchmarks/native_jit.c` stays an experiment).
- `kiln` is not blocked on this (see "kiln" below).

## What "Lua-like" means here

Lua is not indentation-sensitive. Its blocks are delimited by keywords (`then`/`do`/`end`), and whitespace does not matter. That is what "the same indentation as Lua" means in this plan: **keyword-delimited blocks, written with Lua's usual indentation style**. If I ever want Python-style significant indentation instead, that is a different and harder lexer change (INDENT/DEDENT tokens, awkward with multi-line lambdas and the REPL) and needs a new decision.

Sketch of the direction **(proposed, not final)**:

```text
-- today                                  -- Lua-like Ash
fn make_adder(x) {                        fn make_adder(x)
    return fn(y) { return x + y; };           return fn(y) return x + y end
}                                         end

if (n % 2 == 0) {                         if n % 2 == 0 then
    print "even";                             print "even"
} else {                                  else
    print "odd";                              print "odd"
}                                         end

while (i < n) { i = i + 1; }              while i < n do i = i + 1 end

for (x in nums) { print x; }              for x in nums do print x end

try { ... } catch (e) { ... }             try ... catch e ... end
```

Likely rules: statements end at a newline or `;` (semicolons optional), conditions need no parentheses, `end` closes every block, comments stay as they are or move to `--` **(open: keep `//` or switch to `--`?)**.

## What must stay (do not change)

These are the things that make Ash good and fast. The syntax work must leave them alone.

**Runtime and performance**
- The bytecode `Chunk` format, the stack VM and the computed-goto dispatch loop (`ashvm/src/vm/`).
- The hand-fused superinstructions (`x = x + y`, `while (i < n)` and similar). They are why loops are fast.
- Value representation (`VMValue` tagged type, real `double` numbers), the hash map, and builtins' behavior.
- Closures with **by-value capture**, first-class functions, `map`/`filter`/`reduce`, `try`/`catch`, `for (x in array)`.

**Behavior users see**
- Rust-style diagnostics: `file:line:column`, a source snippet, a caret and a contextual hint, for both compile-time and runtime errors. New syntax must produce equally good errors. For example, a missing `end` must point at the block that was never closed, not at end-of-file.
- Import semantics: `@import <./relative.ash>` and `@import <lib>` (resolved through `ash.libs/<lib>/<lib>.ash`), relative to the **entry file's directory**, with namespaced access and circular/collision errors.
- The REPL, including persistent variables across lines.

**Project structure**
- `AGENTS.md` layout and rules. Split new code into feature folders, mirrored across `C/H/D/O`.
- The regression harnesses (`tests/scripts/run_regression.sh`) as the safety net.

## Performance rules (non-negotiable)

Today's baseline, measured 2026-10-07 on my laptop (median of 5 runs; the laptop is slow, so only **relative** comparisons on the same machine matter):

| Benchmark | `ashvm` |
|---|---|
| `fib(30)` | 0.178s |
| sum 0..20M (`loop`) | 1.36s |
| `loop_varbound` | 1.37s |
| `countdown` | 1.36s |
| `hashmap` (50k inserts) | 0.081s |

The README's older numbers (0.06s / 0.12s) do not reproduce here. Whether that is machine difference, staleness or a fusion that is not firing is **(open)**; see "Investigate first".

Rules:
1. **Measure before and after**, on the same machine, with the same method (median of at least 5 runs, `ashvm/benchmarks/*.ash`). A change that makes any benchmark more than ~5% slower does not land.
2. **The syntax change must not touch the VM.** Work is limited to the lexer and compiler (`ashvm/src/lexer`, `ashvm/src/compiler`). New syntax compiles to the **same bytecode** as the equivalent old-syntax program. The same bytecode means the same speed.
3. **Superinstruction fusion must keep firing.** The compiler's fusion peeks at tokens after `(` in conditions (`try_fuse_condition`). When parentheses become optional, `while i < n do` and `x = x + y` must still match those patterns. This is the single most likely way to lose speed by accident, so test it explicitly.
4. **No per-instruction cost.** No extra checks, indirection or bookkeeping in `vm/dispatch.c` for the sake of syntax.
5. Benchmarks are converted to the new syntax too, and compared against their old-syntax timings.

## How (proposed phases)

0. **Investigate first (optional, small).** Find out why `loop` takes ~1.36s and whether the `while (i < n)` fusion is firing. Fixing that first gives the rework a better baseline, and it settles the README numbers.
1. **Freeze the safety net.** Capture regression baselines (`run_regression.sh baseline`) and benchmark timings. If there is no bytecode dump, consider adding a small one so "same bytecode" can be checked directly rather than inferred from timings.
2. **Lexer.** Add the new keyword tokens (`then`, `do`, `end` or whatever is chosen). Make `;` optional if decided.
3. **Compiler.** `block()` is the single choke point for blocks. Rework it to stop at `end`/`else` instead of `}`, then update `if`, `while`, `for`, `try`/`catch`, `fn` and lambda emission (`stmt.c`, `stmt_control.c`, `calls.c`, `expr.c`). Make parentheses around conditions optional without breaking fusion.
4. **Diagnostics.** Update messages and hints for the new tokens ("expected `end` to close this `if`"), keeping the Rust-style output.
5. **Migrate** all `.ash` files: `tests/cases/**`, `benchmarks/*.ash`, README examples. This is mechanical. Run the regression suite after: output must match except where a diagnostic prints new syntax.
6. **Compare** benchmarks against the baseline above. If anything regressed, fix before moving on.
7. **Docs/tooling:** update `README.md`, `CHANGELOG.md`, and the VS Code syntax extension (separate repo, `ash-syntax`).

Each phase keeps the build and tests green and is verified before the next one starts.

## kiln

`kiln` (the native `.ash` → ELF/PE compiler) has its **own** lexer and parser, so it does not get the new syntax for free. **(open)** Port it too (about the same amount of frontend work again), or leave kiln on the old syntax for now. If it stays behind, the two will accept different languages until it is ported, so this must be a conscious choice and recorded here.

## Old syntax: keep or drop? (open)

**Proposed:** replace it outright at the switch. Supporting both doubles lexer/compiler complexity and costs roughly 50% more effort, and the old brace syntax has no reason to survive a rework whose point is to leave it. The cost is that every `.ash` file in the repo (and anyone else's scripts) has to be migrated at once, which the phases above already plan for.

## Definition of done

- Programs are written in the Lua-like syntax and read like Lua.
- Full regression suite passes for the migrated tests (identical output, apart from diagnostics that now print new syntax).
- Benchmarks are within ~5% of the baseline (or better, if phase 0 improved it).
- Diagnostics for syntax errors are as good as today's.
- `AGENTS.md` structure rules still hold; docs, changelog and editor extension are updated.

## Open questions

1. Keyword set and comment style (`//` vs `--`)?
2. Semicolons optional, or removed entirely?
3. Old syntax: replace at once (recommended) or support both for a while?
4. Port `kiln` now or later?
5. Does "high level" also mean new language features (classes, varargs, default parameters, a larger standard library) after the syntax change? If so, that gets its own plan.
