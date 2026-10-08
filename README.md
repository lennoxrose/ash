<div align="center">

# Ash

<img src="logo.svg" width="180" alt="Ash logo" />

</div>

---

Ash is a small programming language built from scratch in C - lexer,
compiler, and a bytecode VM, all hand-written with no external dependencies.
It has closures, hash maps, first-class functions, real floating-point
numbers, `attempt`/`handle`, and a colorized compiler that points straight at
your mistakes.

```ash
forge make_adder(x) {
    yield forge(y) { yield x + y; };
}

local add5 = make_adder(5);
say add5(3); // 8

local nums = [1, 2, 3, 4, 5];
local evens = filter(nums, forge(n) { yield n % 2 == 0; });
say evens; // [2, 4]

attempt {
    local m = {"name": "Ash"};
    say m["missing"];
} handle (e) {
    say "caught: " + e;
}

// conditions can drop the parens, and logical operators read as words
given add5(3) > 7 and not (evens[0] == 1) {
    say "Lua-alike, not a Lua copy";
}

each n in evens {
    say n;
}

// stop/next, compound assignment, ternary, bitwise ops, and/none
local total = 0;
local i = 0;
during i < 10 {
    i += 1;
    given i == 3 { next; }
    given i == 7 { stop; }
    total += i;
}
say total > 0 ? "positive" : "zero or less"; // ternary
say (5 & 3) | (1 << 2); // bitwise: and, or, shift

local maybe = none;
say maybe == none ? "nothing there" : "something there";
```

## Engine

Ash compiles to real bytecode and runs it on a stack-based VM (`ashvm`) with
computed-goto instruction dispatch and a handful of hand-fused
"superinstructions" for common patterns like `x = x + y` and `during (i < n)`
that collapse several bytecode ops into one. Ash used to ship a second,
slower tree-walk interpreter (`ashc`) as the place new features were
prototyped first - it has since been retired now that the VM has full
feature parity, including real `double` number semantics and Rust-style
runtime diagnostics.

## Performance

Benchmarked against equivalent programs in C++ (`g++ -O2`) on the same machine:

| Benchmark | C++ | `ashvm` |
|---|---|---|
| `fib(30)` (recursive) | 0.005s | 0.06s (~12x slower) |
| Sum of 0..20,000,000 | 0.004s | 0.12s (~30x slower) |

That speed comes from three things layered on top of each other: compiling
to real bytecode instead of re-parsing source on every loop iteration,
computed-goto instruction dispatch instead of a `switch` statement, and
those hand-fused superinstructions. A hand-written x86-64 JIT experiment
(`benchmarks/native_jit.c`) that skips bytecode entirely gets within ~2x of
raw C++, which is roughly the ceiling for anything short of a full
optimizing native compiler.

## Language features

- Numbers (real doubles, with `1e3`-style literals), strings (with escape sequences), arrays, hash maps, `none`
- A real boolean type: `yes` / `no`, and everything that answers a question (`==`, `<`, `not`, `and`, `or`, `has`, `contains`, `file_exists`, ...) produces one. They print as `yes` / `no`, `type(yes)` is `"boolean"`, and `yes == 1` is false. Conditions accept booleans, numbers and `none`; a string, array, map or function in a condition is an error
- `and` / `or` short-circuit; strings compare bytewise with `< <= > >=`
- Maps keep insertion order (`keys`, `values`, printing); `delete` closes the gap
- Functions, recursion, closures with by-value capture, and forward declarations: a function can be called before its `forge` appears, so mutual recursion just works
- First-class functions - pass them around, store them in variables, call them indirectly. Builtins are values too: `map(xs, str)`, `local f = len;`
- Builtins:
  - core: `len type str num exit sqrt abs floor chr ord`
  - collections: `push pop insert slice delete sort keys values has map filter reduce` (`delete(array, i)` removes an index, `sort(array)` / `sort(array, cmp)` sorts in place and is stable; `cmp(a, b)` returns a number, `> 0` meaning `a` goes after `b`)
  - strings: `split join substring indexOf contains starts_with ends_with replace repeat upper lower trim`
  - files: `read_file write_file append_file file_exists rename_file delete_file make_dir list_dir`
- One number-to-text format on both engines: integers print plainly up to 1e15, other values with up to 6 fraction digits, and anything beyond that (or below 1e-6) in scientific form (`1e+21`, `1.5e-07`)
- `attempt` / `handle` error handling. Built-in errors raise a string with the same wording on both engines (`index out of bounds`, `key not found`, `invalid operand type in '+'`, ...); `raise expr;` raises any value, so a map gives a structured error. `e.message` (more generally `var.field`, same as `var["field"]`) reads a field of a raised map, and on a plain string error it is the string itself, so `e.message` works for every error
- `each (x in array)` and `during` loops, with `stop` / `next` for early exit and skip-to-next-iteration (leaving an `attempt` block that way pops its handler)
- Compound assignment (`+= -= *= /=`) and `++` / `--`
- Ternary `cond ? a : b`
- Bitwise operators: `& | ^ ~ << >>`
- Imports: `@import <./file.ash>;` makes the file's functions callable as `file.name(...)`; `@import <./file.ash> as alias;` picks the namespace. Imports are transitive (a file imported by a file you import is callable by its own namespace too) and each file is loaded once. A top-level `local` in an imported file is a module-level variable: its expression runs once at import, and the file's functions can read and update it (`NAME = ...;`, `NAME += ...;`, `NAME[i] = ...;`); outside, `file.NAME` reads it
- `exit(code)` ends the program with that status; an uncaught error exits with status 1
- Colorized, Rust-style diagnostics for both compile-time and runtime
  errors: file:line:column, a source snippet, a caret, and a contextual hint
- An interactive REPL with persistent variables across lines

### Performance notes

- `len(s)`, `substring(s, i, j)` and `s[i]` are O(1) / O(length of the piece) on both engines (strings carry their length).
- `s += x` copies `s`, so building a long string one piece at a time is quadratic in time and, since nothing is freed, in memory. Collect the pieces in an array and `join(parts, "")` once - that is linear. `kiln`'s heap grows as needed (64MB at a time, `brk` on Linux / `VirtualAlloc` on Windows) rather than crashing at a fixed size, but nothing is freed until the process exits either way.
- `push` grows arrays geometrically (amortized O(1)); `sort` is a stable merge sort on `ashvm` and a stable insertion sort on `kiln`.
- `kiln`'s Windows output is checked without a Windows machine: `kiln/tests/scripts/windows_emulated_check.sh` runs every test case as a PE under the Unicorn x86-64 emulator (kernel32 faked over the real filesystem) and compares it with the Linux build. All of them, including the Windows file operations (`list_dir` via `FindFirstFileA`/`FindNextFileA`), match. `kiln/tests/scripts/windows_wine_check.sh` does the same under Wine (an independent `kernel32`), also with all cases matching. Neither is Windows itself.

## Getting started

```bash
cd ashvm && make          # builds ../bin/ashvm

../bin/ashvm script.ash   # run a script
../bin/ashvm              # start the REPL
```

`kiln` (compiles `.ash` straight to a native ELF/PE binary) and `forgepack`
(the package manager) build the same way, each from its own directory:
`cd kiln && make` / `cd forgepack && make`.

## Project structure
ashvm/          the engine - everything below is self-contained inside here
src/
lexer/        tokenizer
value/        VMValue tagged type, hash map
vm/           bytecode Chunk format, dispatch loop, error handling, public API
compiler/     single-pass compiler: parser/emitter state, expression chain,
call/lambda emission, statement compilation
builtins/     len, map, filter, reduce, string/file ops
diagnostics/  shared Rust-style error reporting (compile-time and runtime)

Every subsystem keeps sources in `C/`, headers in `H/`, build output in a
mirrored `O/` tree, and generated dependency files in `D/` (so editing a
shared header correctly triggers a rebuild of everything that includes it),
e.g. `src/vm/C/dispatch.c` includes `src/vm/H/vm.h` and builds to
`src/vm/O/dispatch.o` with `src/vm/D/dispatch.d` tracking the header.
app/          main.c entry point (file mode + REPL)
benchmarks/     .ash and .cpp benchmark programs, plus the x86-64 JIT experiment
tests/          cases/<feature>/*.ash programs covering each language feature; scripts/ holds the regression runners

kiln/           a second, independent implementation: compiles .ash straight to a
native ELF/PE binary (no VM, no libc), same language, full feature parity
with ashvm, cross-validated against it directly (see CHANGELOG.md).

forgepack/      package manager -- `forgepack init/add/install/list`, dependencies
fetched from GitHub into a project-local @ash-modules/ folder (same idea
as node_modules). `@import <name>;` already works with it. No registry
yet -- see ideas/ash_modules.md for the full design and what's next.

## Editor support

A VS Code extension with syntax highlighting is at https://github.com/lennoxrose/ash-syntax
A VS Code extension with the Icon pack is at https://github.com/lennoxrose/ash-icons/releases/tag/v1.2.12vsix