<div align="center">

# Ash

<img src="logo.svg" width="180" alt="Ash logo" />

</div>

---

Ash is a small programming language built from scratch in C - lexer,
compiler, and a bytecode VM, all hand-written with no external dependencies.
It has closures, hash maps, first-class functions, real floating-point
numbers, `try`/`catch`, and a colorized compiler that points straight at
your mistakes.

```ash
fn make_adder(x) {
    return fn(y) { return x + y; };
}

let add5 = make_adder(5);
print add5(3); // 8

let nums = [1, 2, 3, 4, 5];
let evens = filter(nums, fn(n) { return n % 2 == 0; });
print evens; // [2, 4]

try {
    let m = {"name": "Ash"};
    print m["missing"];
} catch (e) {
    print "caught: " + e;
}
```

## Engine

Ash compiles to real bytecode and runs it on a stack-based VM (`ashvm`) with
computed-goto instruction dispatch and a handful of hand-fused
"superinstructions" for common patterns like `x = x + y` and `while (i < n)`
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

- Numbers (real doubles), strings (with escape sequences), arrays, hash maps
- Functions, recursion, closures with by-value capture
- First-class functions - pass them around, store them in variables, call them indirectly
- `map`, `filter`, `reduce`, and 20+ other built-ins (string ops, file I/O, math)
- `try` / `catch` error handling
- `for (x in array)` and `while` loops
- Colorized, Rust-style diagnostics for both compile-time and runtime
  errors: file:line:column, a source snippet, a caret, and a contextual hint
- An interactive REPL with persistent variables across lines

## Getting started

```bash
cd ashvm && make          # builds ../bin/ashvm

../bin/ashvm script.ash   # run a script
../bin/ashvm              # start the REPL
```

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

Every subsystem keeps sources in `C/`, headers in `H/`, and build output in a
mirrored `O/` tree (kiln also has `D/` for dependency files), e.g.
`src/vm/C/dispatch.c` includes `src/vm/H/vm.h` and builds to `src/vm/O/dispatch.o`.
app/          main.c entry point (file mode + REPL)
benchmarks/     .ash and .cpp benchmark programs, plus the x86-64 JIT experiment
tests/          cases/<feature>/*.ash programs covering each language feature; scripts/ holds the regression runners
kiln/           a standalone .ash compiler (Windows + Linux). Not started yet.

## Editor support

A VS Code extension with syntax highlighting is at https://github.com/lennoxrose/ash-syntax
A VS Code extension with the Icon pack is at https://github.com/lennoxrose/ash-icons/releases/tag/v1.2.12vsix

## Future of Ash

Read the plans.md for further plans about Ash