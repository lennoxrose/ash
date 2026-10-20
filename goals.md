# Goals: what Ash still needs to be a fully fledged language

Ordered by importance (1 = most important). Status: `[ ]` open, `[~]` partly done, `[x]` done.

Time estimates are for Claude doing the work in this repo: implementation on both engines
(`ashvm` and `kiln`), tests in both `tests/cases/` trees, regression runs, README and CHANGELOG.
They are focused working time, not calendar time, and they assume you review in between.
`kiln` is usually the expensive half (no libc, hand-emitted x86-64), so items touching
runtime behaviour carry most of their cost there.

| # | Goal | Status | Estimate |
|---|---|---|---|
| 1 | Memory management: Rust-grade safety, off-switch in `ash.pkg` | [~] | 1-2 weeks |
| 2 | Standard library: math | [~] | 1-2 hours |
| 3 | Standard library: time, args, env, stdin | [~] | 3-5 hours |
| 4 | String formatting and interpolation | [x] | 3-4 hours |
| 5 | Structs / methods | [x] | 1-1.5 days |
| 6 | Closures that mutate captured variables | [ ] | 4-8 hours |
| 7 | `switch` / pattern matching | [~] | 4-6 hours |
| 8 | Integer type separate from double | [x] | 1-2 days |
| 9 | Test framework and stack traces | [x] | 4-6 hours |
| 10 | Formatter and LSP | [~] | 2-4 days |
| 11 | forgepack registry, versions, lockfile | [~] | 2-3 days |
| 12 | Process execution and networking | [~] | 2-3 days |
| 13 | Concurrency | [ ] | 3-5 days |
| 14 | Self-hosting compiler | [ ] | 1-2 weeks |

### Progress (2026-10-21)
The language is now the Java-like Ash (`hearth/front` is the only frontend; the old syntax is the internal IR), so the
items below were judged against that. Evidence is in `CHANGELOG.md`.

- **Done (`[x]`)**
  - **4** `${}` interpolation, `String.format` / `formatted` / `printf`.
  - **5** classes, records, enums, generics and methods.
  - **8** exact 64-bit `long`, wrapping `int`, `float32`, exact double printing.
  - **9** `ash.test.Assert` (in `ash-stdlib`) and call stacks for uncaught exceptions on both engines.
- **Partly done (`[~]`)**
  - **1** see its status block.
  - **2** `sqrt` builtin plus `ash.math.Numbers` (`PI`, `pow`, `gcd`, `clamp`, `round`); missing `sin cos tan atan2 log exp`, `random`.
  - **3** `ash.cli.Args`, `ash.time.Duration`, `getenv` and stdin reading exist; the stdlib time/env models have no real clock or environment behind them yet.
  - **7** `switch` statements and expressions with type patterns, `when` guards, fallthrough; ranges and destructuring not done.
  - **10** `ashvm fmt` and `ashvm lsp` (diagnostics, hover, go to definition); completion missing.
  - **11** `ash.lock`, `apm update` / `remove`, transitive dependencies, hub login and `owner/repo` installs; semver range resolution not done.
  - **12** `sys_fork` / `sys_wait` / `sys_exec` on `kiln`; no sockets (`ash.net.Http` always fails).
- **Open (`[ ]`)**
  - **6** lambdas keep Java's effectively-final rule; mutating captured variables is not done.
  - **13** `ash.concurrent` is an API model that runs tasks on the caller, not real concurrency.
  - **14** nothing started.

## 1. Memory management: Rust-grade safety, switchable off (1-2 weeks)
Nothing is ever freed. `s += x` in a loop and every array/map/closure leaks, and `kiln` has a
fixed 16 MB bump heap, so long-running or data-heavy programs crash. This is the one gap that
caps what programs can be written at all.

**Design goal:** memory safety as strong as Rust's (no use-after-free, no double free, no
dangling references, no leaks in safe code, no data races once item 13 exists), **but never a
limit**: a project can turn it off in `ash.pkg` and then neither `ashvm` nor `kiln` enforces or
pays for any of it.

### Status (2026-10-18)
Done: the `memory-safety` switch in `ash.pkg` (default `on`, kept by `apm`); ownership with moves on stores and on
passing to project code, `&x` / `&mut x` borrows as call arguments, control-flow-aware use-after-move errors; sole-owner
freeing with no collector (`ashvm`: Strings, arrays, objects, factory results; `kiln`: Strings, arrays and objects, static Linux builds);
`System.free` for `off`; constant `int` overflow and constant out-of-bounds indexes denied at compile time; the
shared Rust engine `bin/lib/mnemo.a`. Bounded memory for a loop of millions of short-lived strings holds on both
engines (`tests/scripts/memory_check.sh`). Parameters a method only reads are borrowed automatically (inferred, no syntax). Not done: borrow-typed parameters and lifetimes
for `&x` passed to a callee that keeps it (the compiler never frees such a value, so this costs memory, not safety), by-move closure capture,
freeing array elements and fields that can be shared, the blocks `kiln`'s `push` abandons, `kiln` on Windows /
`--link=shared`.

### The switch
```text
name: my-project
version: 0.1.0
memory-safety: on        # on (default) | off

[dependencies]
```
- Read from the `ash.pkg` at the project root, the same root `@import <name>;` already uses, so
  `ashvm`, `kiln` and `forgepack` agree on one source of truth. No `ash.pkg` = `on`.
- The setting is per project and applies to the whole build, including imported libraries
  (a safe library compiled into an unsafe project is simply unchecked; an unsafe project never
  makes a library's own tests behave differently, because libraries are tested in their own project).
- `off` means: the compilers skip the ownership/borrow pass, the runtimes skip use-after-free,
  bounds and double-free checks, and nothing is reclaimed automatically. Memory is then yours:
  `free(x)` is available and the program may do anything a C program can, including crash.
- `on` and `off` must produce the same output for every program that is valid under `on`
  (tested by running the regression suites in both modes).

### How `on` works (Rust model, adapted to a dynamic language)
- **Ownership:** every heap value (string, array, map, function environment) has exactly one
  owner. Assigning or passing a heap value *moves* it; the old name becomes unusable and using
  it is a compile-time error. Values are freed deterministically when the owner goes out of
  scope, so there is no collector and no pause.
- **Borrowing:** `&x` is a shared borrow, `&mut x` an exclusive one, with Rust's rule (many
  readers or one writer, never both) and a borrow cannot outlive its owner. Checked at
  compile time by a pass over the compiler's existing scope tracking.
- **Copy and clone:** numbers, booleans and `none` are plain copies. `clone(x)` is an explicit
  deep copy, which is how you keep using a value after handing it on.
- **Closures:** a closure takes the variables it uses by move, or by borrow if it does not
  outlive them. This also gives item 6 (mutating captured variables) a sound answer.
- **Runtime backstop:** anything the static pass cannot prove (values reached through
  `any`-typed containers, indexing, `keys`/`values` results) falls back to a cheap runtime
  check that raises a catchable error instead of reading freed memory.

### The honest difficulty
Rust can do this statically because every variable has a type. Ash is dynamically typed, so a
full borrow checker would be unsound or would reject valid programs unless it is paired with
the runtime backstop above or optional type annotations (which are on the "nice to have" list).
The first milestone is therefore the strict subset that is provable without types (moves,
scope-based freeing, borrows of local variables), and everything else is checked at run time
under `on`. Reaching "as safe as Rust" in the full sense, including through containers and
function boundaries, is a research-sized part of this item and may need item 10's type
information; I will report where the line falls rather than claim more than is proven.

### Work plan
1. `ash.pkg` reading in `ashvm` and `kiln` (small): a shared parse of `memory-safety`, plus the
   flag plumbed to the compiler/codegen; `forgepack init` writes the key.
2. Allocator with deterministic free on both engines: `ashvm` frees on scope exit via the
   compiler-emitted drop points (no cost on the hot instruction paths; `fib(30)` ~0.06s and
   sum to 20M ~0.12s must not regress); `kiln` replaces the bump heap with a free-list allocator
   emitted into the binary. Most of the time goes here.
3. Move semantics and use-after-move errors in the compiler (Rust-style diagnostics with the
   existing renderer).
4. Borrow checking for locals, then for function arguments and closures.
5. Runtime backstop checks, all behind the same flag so `off` compiles them out.
6. `off` mode: `free()`, no checks, regression suites in both modes.
- Done when a loop building 10M short-lived strings/arrays runs in bounded memory on both
  engines in both modes, use-after-move and aliasing-while-mutating are rejected under `on`,
  and `off` runs the same programs without any of those checks.

## 2. Standard library: math (1-2 hours)
Only `sqrt abs floor` exist. Add `ceil round pow min max sin cos tan atan2 log exp`, `random` / `seed`,
and constants. `ashvm` is trivial (libm). `kiln` has no libc, so `sin/cos/log/exp` need x87 or
SSE instruction sequences; `pow`, `min`, `max`, `ceil`, `round` are cheap.

## 3. Standard library: time, args, env, stdin (3-5 hours)
`clock()` / `now()`, `args()`, `env(name)`, `read_line()` / `read_all()` from stdin. All are
thin syscalls on Linux (`clock_gettime`, argv/envp from the initial stack, `read(0)`); on
Windows `kiln` they need new kernel32 imports (`GetTickCount64`/`GetSystemTimeAsFileTime`,
`GetCommandLineA`, `GetEnvironmentVariableA`, `ReadFile` on stdin) and emulator/Wine checks.

## 4. String formatting and interpolation (3-4 hours)
Building text is `"a" + str(x) + "b"` today. Add either interpolation (`"x = {x}"`, a lexer and
compiler change that desugars to `join`, zero VM cost) or a `format("{} {}", a, b)` builtin, ideally both.
Padding/precision control for numbers rides along.

## 5. Structs / methods (1-1.5 days)
Records are maps and `var.field` is only sugar. Wanted: a declaration form, field-name checking
at compile or run time, and `obj.method(args)` with an implicit receiver. Decide the model
first (struct + `impl`-style methods vs. map-with-functions sugar); that decision is the main
cost. The compiler work mirrors the existing module/namespace code; `kiln` needs matching codegen.

## 6. Closures that mutate captured variables (4-8 hours)
Closures capture by value, so `local n = 0; forge inc() { n++; }` does not update the outer `n`.
Needs boxed (shared) cells for captured variables that are assigned after capture: compiler
analysis to find them plus a cell value type on both engines. Interacts with item 1 (cells are heap objects).

## 7. `switch` / pattern matching (4-6 hours)
A `match`-style statement over literals, ranges and types, with a default arm. Pure
compiler lowering to the existing compare/jump ops, so no VM cost. Destructuring
(`local [a, b] = pair;`) and spread fit in the same pass.

## 8. Integer type separate from double (1-2 days)
Everything is a double, so integers above 2^53 lose precision and bit ops go through
conversion. A real `int` (64-bit) with exact arithmetic and overflow rules touches every
numeric builtin, number formatting, `==`, and both engines' value tags. High risk for the
regression diffs and the performance budget; do it after 1 and 6, and only if the need is real
(hashes, file sizes, timestamps in ns).

## 9. Test framework and stack traces (4-6 hours)
A built-in `assert(cond, msg)` / `assert_eq` plus a small runner convention so libraries stop
writing their own (ash-json has its own). Runtime errors should print the call stack
(function names and lines), which also helps everything else on this list.

## 10. Formatter and LSP (2-4 days)
`ash fmt` is a lexer plus a pretty-printer (about a day). A language server (diagnostics from
the existing compiler errors, go-to-definition, completion over builtins/imports) is the rest.
Needs the compiler to expose a position-aware parse without running code.

## 11. apm registry, versions, lockfile (2-3 days, partly done)
Done: `init`, `add github:...`, `add owner/repo[@version]` via the APMH hub, `install [--locked]`, `update`, `remove`,
`list`, `ash.lock`, transitive dependencies, auto-created `ash.pkg`. Missing: semver range
resolution and a registry beyond the hub (`ideas/ash_modules.md` has the plan). The server side is out of scope for me to host; I can
write the client and the index format.

## 12. Process execution and networking (2-3 days)
`run("cmd", args)` returning output/status, then TCP sockets (`connect`, `listen`, `send`,
`recv`). Linux is raw syscalls on `kiln`; Windows needs ws2_32 imports and a Winsock
stand-in for the emulator check. HTTP/TLS would be a library on top, TLS being a large project of its own.

## 13. Concurrency (3-5 days)
Choose a model first: cooperative tasks/coroutines (cheapest, fits the VM, `yield`-style
switching of frames) vs. OS threads (needs thread-safe heap, so it depends on item 1 being
done in a thread-aware way). I would start with coroutines. `kiln` needs stack switching in generated code.

## 14. Self-hosting compiler (1-2 weeks)
Write the lexer/compiler in Ash and compile it with `kiln`. Not useful by itself, but it is
the real test that items 1-9 are sufficient and it exercises performance. Only start once
1, 2, 3, 4, 5 and 9 are done.

## Suggested order of work
1. Items 2, 3, 4 first: cheap, immediately useful, and they unblock real programs.
2. Item 1 next: the largest correctness risk; everything allocating gets safer once it exists.
3. Items 6, 5, 7: language ergonomics, once memory is sound.
4. Item 9, then 11 and 10: the tooling that makes the ecosystem usable.
5. Items 8, 12, 13, 14 as demand appears.

Estimates are rough: item 1 and item 8 could be off by 2x either way, because the `kiln`
side has no existing runtime to extend. Each item should land as its own change with a
baseline and check of both regression suites, and a CHANGELOG entry.