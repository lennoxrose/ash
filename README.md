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

- Numbers (real doubles), strings (with escape sequences), arrays, hash maps, `none`
- `yes`/`no` boolean literals (plain sugar for `1`/`0` - truthiness is number-based throughout)
- Functions, recursion, closures with by-value capture
- First-class functions - pass them around, store them in variables, call them indirectly
- `map`, `filter`, `reduce`, `type` (runtime type introspection: `"number"/"string"/"array"/"map"/"function"/"none"`), and 20+ other built-ins (string ops, file I/O, math)
- `attempt` / `handle` error handling, plus `raise expr;` for user-thrown errors with a runtime string message
- `each (x in array)` and `during` loops, with `stop` / `next` for early exit and skip-to-next-iteration
- Compound assignment (`+= -= *= /=`) and `++` / `--`
- Ternary `cond ? a : b`
- Bitwise operators: `& | ^ ~ << >>`
- Colorized, Rust-style diagnostics for both compile-time and runtime
  errors: file:line:column, a source snippet, a caret, and a contextual hint
- An interactive REPL with persistent variables across lines

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