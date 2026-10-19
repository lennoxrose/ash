<div align="center">

# Ash

<img src="logo.svg" width="180" alt="Ash logo" />

</div>

---

Ash is an enterprise-style, statically typed, Java-like language built from scratch in C with no
external dependencies: packages, classes, interfaces, enums, records, generics, `T?` null safety,
checked exceptions, lambdas.

```java
package demo;

import ash.util.List;
import ash.util.ArrayList;

public class Main {
    record Point(int x, int y) { }

    public static void main(String[] args) {
        List<Point> points = new ArrayList<>();
        points.add(new Point(1, 2));
        for (Point p : points) {
            System.out.println(p + " " + (p.x() + p.y()));
        }
    }
}
```

## Getting started

```bash
make                                    # builds bin/ashvm, bin/kiln and the rest (`make @ashvm`, `make @kiln` for one)

bin/ashvm run Main.ash                  # run a program on the VM
bin/ashvm run --wrap-int Main.ash       # Java's 32-bit int overflow (slower arithmetic loops)
bin/ashvm check Main.ash                # parse and type-check only
bin/ashvm                               # interactive REPL

bin/kiln Main.ash -o main               # compile to a native ELF binary (no VM, no libc)
bin/kiln Main.ash -o main --target=windows --link=shared --wrap-int
```

The standard library is not built into the compiler. The compiler only contains the types it cannot work
without (`ash.lang`, the functional interfaces, `ash.util.Iterator`); `ash.util` (collections, `Optional`,
`Stream`), `ash.io`, `ash.json`, `ash.math`, `ash.time`, `ash.cli`, `ash.test` and the model packages live
in the separate `ash-stdlib` package. Install it with apm (`apm add github:<user>/ash-stdlib`,
which lands in `./@ash-modules/ash-stdlib` and is found automatically), or set
`ASH_PATH=/path/to/ash-stdlib/src/main/ash`. Without it, programs that only use `ash.lang` still work and
anything else reports a `cannot find type` error that names the package.

`ashvm` and `kiln` are the only front doors. Both link the one shared front end in `hearth/front`
(lexer, parser, AST, type checker, lowering) and differ only in the back end: a bytecode VM
versus a native code generator. Source written in the removed pre-migration scripting syntax
(`local`, `say`, `given`, `forge`, ...) is rejected with a message naming the new construct.

`examples/` has 18 runnable programs (classes, enums, records, generics, collections, lambdas,
exceptions, file I/O, JSON, CLI, game, server, desktop-style); `hearth/ashvm/tests/scripts/run_regression.sh`
and `hearth/kiln/tests/scripts/run_regression.sh` run them on both engines. The standard library is the
`ash-stdlib` package (`ash.util`, `ash.io`, `ash.json`, `ash.math`, `ash.cli`, `ash.test`, `ash.time`,
`ash.concurrent`, `ash.net`, `ash.web`, `ash.game`, `ash.desktop`; the last five are pure-Ash
models because the VM has no threads, sockets, windows or clock); the test scripts expect it as a sibling
checkout (`../ash-stdlib`) or via `ASH_PATH`.

### REPL

`ashvm` with no arguments starts a Java-like REPL. Imports, type declarations (`class`, `interface`,
`enum`, `record`), static methods (`int sq(int a) { return a * a; }`) and statements persist between
lines; a line without a trailing `;` or `}` is evaluated and printed (`x + 2`). Each line is checked and
run in a child process, so a compile error or uncaught exception leaves the session intact.
Limits: session state is rebuilt by replaying earlier lines (output suppressed), so non-idempotent side
effects such as file writes, stdin reads and clocks repeat; a variable name can be declared only once;
diagnostic line numbers refer to the generated wrapper, not to what you typed.

## Engine

Ash lowers to an internal representation that `ashvm` compiles to real bytecode and runs on a
stack-based VM with computed-goto instruction dispatch and a handful of hand-fused
"superinstructions" for common patterns like `x = x + y` and `while (i < n)` that collapse several
bytecode ops into one. `kiln` compiles the same representation straight to machine code.
That representation is a compiler internal (`hearth/front/src/ir`), not Ash source; no tool reads it
from a file.

## Performance

Benchmarked against equivalent programs in C++ (`g++ -O2`) on the same machine:

| Benchmark | C++ | `ashvm` |
|---|---|---|
| `fib(30)` (recursive) | 0.005s | 0.25s |
| Sum of 0..20,000,000 | 0.004s | 1.5s |

(`hearth/ashvm/benchmarks`, run with `ashvm run`; the numbers include about 4 ms of start-up for
loading and lowering the reachable standard library.) The speed comes from compiling to bytecode
instead of re-parsing source on every loop iteration, computed-goto dispatch instead of a `switch`,
and the fused superinstructions. A hand-written x86-64 JIT experiment
(`benchmarks/native_jit.c`) that skips bytecode entirely gets within ~2x of raw C++, which is
roughly the ceiling for anything short of a full optimizing native compiler.

### Performance notes

- Building a long string one piece at a time with `+=` copies each time, so it is quadratic in time. Collect the pieces and join them once (`StringBuilder`, `String.join`).
- `kiln`'s heap grows as needed (64MB at a time, `brk` on Linux / `VirtualAlloc` on Windows) rather than crashing at a fixed size, but with `memory-safety: on` it gives back the Strings, one-dimensional arrays, objects, maps and sets the compiler can prove unshared (see Memory safety). `ashvm` frees the same kinds and lists; it also has an opt-in collector (`make GC=1`).
- `kiln`'s Windows output is checked without a Windows machine: `hearth/kiln/tests/scripts/windows_emulated_check.sh` runs every test program as a PE under the Unicorn x86-64 emulator (kernel32 faked over the real filesystem) and compares it with the Linux build. `hearth/kiln/tests/scripts/windows_wine_check.sh` does the same under Wine (an independent `kernel32`). Neither is Windows itself.

## Memory safety (`memory-safety` in `ash.pkg`)

`memory-safety: on` is the default; `off` switches every check and every free below off and generates exactly the code Ash always did. It is read from the project's `ash.pkg` (no `ash.pkg`: the entry file's directory is the project), and only the project's own sources are checked, never the standard library or dependencies.

With it on, at compile time:

- **Ownership.** Storing an owning variable (`b = a`, `T b = a`, `this.f = a`, `xs[i] = a`, `{a}`) or passing it to project code moves it unless the callee only reads that parameter (the compiler works that out, so `size(a)` twice is fine); using it afterwards is an error until it gets a new value. Library and native calls only look at their arguments. Moves follow `if`, `switch`, `try`, loops, `return`, `throw`, `break` and `continue`.
- **Borrows.** `&x` lends a local (shared), `&mut x` lends it exclusively; both are written only as call arguments. Within one call an exclusive borrow excludes every other use of the same variable, and a loan cannot be handed to a method the compiler can see keeps its parameter (stores, returns or passes it on).
- **Ranges.** The compiler tracks the possible values of `int` / `long` locals and the lengths of arrays (loops and conditions included) and rejects what can only fail: an index that is always negative or always past the end, an `int` operation that always overflows (not under `--wrap-int`), a divisor that is always `0`, a negative array size, and a method that calls itself before any branch. A value that merely *might* be out of range is left to the run-time check, and nothing inside a `try` with a `catch` is reported.
- **Constants.** A constant `int` expression outside the 32-bit range (`2147483647 + 1`) and a constant array index that is negative or past the length of a literal / `final` array are errors (not under `--wrap-int`).

And at run time memory is freed with no collector (`ashvm`: Strings, arrays, lists, objects, maps and sets; `kiln`: Strings, one-dimensional arrays, objects, maps and sets): a local that is the sole owner of a fresh String, array, `ArrayList`, `HashMap` / `HashSet` or plain object (`new T(..)` whose `this` never escapes, or the fresh result of a project method that returns one) and is only read is freed (with the String and array fields that only it ever references) at the end of its scope, before `return`/`break`/`continue`, and before it is reassigned; concatenation temporaries are freed as soon as they are used, and a list or `String[]` of fresh Strings that are only read in harmless ways frees those Strings with it. Everything the compiler cannot prove unshared is simply left alone, so a leak is possible but a use-after-free is not. In `kiln` (Linux static builds), a loop whose body keeps nothing it allocates past the iteration (no calls into project code, no stores of references to anything declared outside the loop; see `hearth/front/src/jown/C/region.c`) also gives the whole iteration's allocations back at once by moving the heap pointer back. A loop building millions of short-lived strings and arrays runs in a few MB.

With `memory-safety: off`, `System.free(x)` frees a String, array or object yourself in `ashvm` (`kiln` ignores it); it is an error with safety on. `kiln` runs the same checks and frees them too (static builds, Linux and Windows; `--link=shared` builds keep the old allocator). The shared move-flow engine is the Rust library `bin/lib/mnemo.a` (`hearth/mnemo`), linked by both engines.

## Package manager (`apm`)

A project is described by `ash.pkg`, a flat `key: value` header plus `[section]` blocks (like `package.json`):

```
name: my-app
version: 1.2.0
description: What it does
author: Ada <ada@example.com>
license: MIT
homepage: https://example.com/my-app
repository: github:ada/my-app
bugs: https://example.com/my-app/issues
main: demo.Main
keywords: ash, cli

[dependencies]
ash-stdlib: github:<user>/ash-stdlib@v0.1.0

[dev-dependencies]
test-kit: github:<user>/test-kit

[libraries]
m: system
vulkan: pkg-config:vulkan

[scripts]
build: ashvm check src/main/ash/demo/Main.ash
test: ashvm run src/test/ash/AllTests.ash
```

```bash
apm init                                   # create ash.pkg
apm add github:user/repo[@ref] [--as name] [--dev]
apm install                                # fetch [dependencies] and [dev-dependencies] into ./@ash-modules/
apm list                                   # metadata, dependencies (and whether installed), libraries, scripts
apm run <script>                           # run a [scripts] entry with sh -c; `apm build` / `apm test` are shorthands
apm license [<id>] [--author <name>] [--year <yyyy>]   # list licenses / write LICENSE and set license: in ash.pkg
```

`apm license MIT` writes a `LICENSE` file and sets `license: MIT` in `ash.pkg`; running it again with another id replaces both. The copyright holder defaults to `author:` (without the `<email>`), the year to the current year. Built in (no network): MIT, Apache-2.0, BSD-3-Clause, BSD-2-Clause, ISC, OFL-1.1, GPL-2.0, GPL-3.0, LGPL-2.1, LGPL-3.0 (texts from the SPDX list; the `-only` forms are accepted as aliases).

`[libraries]` records the external native libraries a package links; apm stores and lists them, but
`ashvm` and `kiln` do not consume them yet. Scripts run with the same trust as `npm run`: only the
project's own `ash.pkg` is consulted, never an installed dependency's.

## Project structure

Every project lives in `hearth/` and builds into `bin/`; the repository root only holds
documentation, `examples/` and the build entry point.

```
hearth/front/     the only frontend: lexer, parser, AST, type checker, lowering, internal IR lexer,
                  REPL session model and the compiler's core types (src/jcore, compiled in as C data).
                  Built as front.a (in bin/lib/), linked by ashvm and kiln. There is no front-end executable.
hearth/ashvm/     the VM engine: compiler (IR -> bytecode), value, vm, builtins, diagnostics, app (CLI + REPL)
hearth/kiln/      the native compiler: IR -> machine code -> ELF/PE (no VM, no libc)
hearth/apm/       the Ash package manager: apm init/add/install/list/run, dependencies fetched from GitHub
                  into a project-local @ash-modules/ folder; point ASH_PATH at an installed source root
                  to compile against it (see ideas/ash_modules.md)
hearth/pyre/      GPU/CPU compute backends (C ABI)
hearth/mnemo/     Mnemosyne, memory safety: a Rust static library (bin/lib/mnemo.a) with a C ABI, linked by ashvm and kiln
```

Every subsystem keeps sources in `C/`, headers in `H/`, build output in a mirrored `O/` tree, and
generated dependency files in `D/` (so editing a shared header correctly triggers a rebuild of
everything that includes it), e.g. `src/vm/C/dispatch.c` includes `src/vm/H/vm.h` and builds to
`src/vm/O/dispatch.o` with `src/vm/D/dispatch.d` tracking the header.

## Editor support

A VS Code extension with syntax highlighting is at https://github.com/lennoxrose/ash-syntax
A VS Code extension with the Icon pack is at https://github.com/lennoxrose/ash-icons/releases/tag/v1.2.12vsix