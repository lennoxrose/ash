# Ash → Enterprise Java-like Ash Migration Checklist

This document defines the target design for Ash and the ordered migration plan.

Ash must become a strictly enterprise-grade, Java-like, high-level, statically typed language.
It must always use classes, packages, explicit types, access modifiers, constructors, interfaces, enums, records, generics, and exceptions.

The old Ash syntax is deprecated. The new syntax is the only supported application-level syntax.

## Design principles

- [x] Ash is object-oriented at the application level.
- [x] Ash is statically typed.
- [x] Ash is explicit and predictable.
- [x] Ash is suitable for backend services, desktop apps, games, CLI tools, and frontend libraries.
- [x] Ash is familiar to Java, C#, Kotlin, and TypeScript developers.
- [x] Ash is more ergonomic than Java, but never script-like.
- [x] Ash is designed for eventual fast native compilation.
- [x] Every program uses a package and a class-based entry point.
- [x] `//` comments remain supported.
- [x] `/* ... */` and `///` comments are supported.

## Target syntax

- [x] Use `package` declarations.
- [x] Use `import` declarations.
- [x] Use `public`, `private`, `protected`, and package-private access modifiers.
- [x] Use `class`, `interface`, `enum`, `record`, `abstract`, `sealed`, and `permits`.
- [x] Use `extends` and `implements`.
- [x] Use `new` for object creation.
- [x] Use `this` and `super`.
- [x] Use `static`, `final`, and `const`.
- [x] Use `public static void main(String[] args)` as the standard entry point.
- [x] Use `return`, `if`, `else`, `else if`, `switch`, `case`, `default`.
- [x] Use `while`, `for`, `do`, `break`, and `continue`.
- [x] Use `try`, `catch`, `finally`, `throw`, and `throws`.
- [x] Use `true`, `false`, and `null`.
- [x] Use `instanceof`.
- [x] Use `var` only for obvious local type inference.
- [x] Use Java-style generics: `List<Player>`, `Map<String, Player>`, `Set<T>`.
- [x] Use lambdas and method references.
- [x] Use `Optional<T>` and `Result<T, E>` where appropriate.

## Keyword migration

Complete this table and implement it consistently:

| Old Ash | New Ash | Status |
|---|---|---|
| `local x = 5;` | `int x = 5;` or `var x = 5;` | [x] |
| `say x;` | `System.out.println(x);` or `Console.println(x);` | [x] |
| `given` | `if` | [x] |
| `otherwise` | `else` | [x] |
| `during` | `while` | [x] |
| `each x in xs` | `for (Type x : xs)` | [x] |
| `stop;` | `break;` | [x] |
| `next;` | `continue;` | [x] |
| `forge name(args)` | method declaration | [x] |
| `yield value;` | `return value;` | [x] |
| `attempt` | `try` | [x] |
| `handle (e)` | `catch (Exception e)` | [x] |
| `raise value;` | `throw new SomeException(...);` | [x] |
| `yes` | `true` | [x] |
| `no` | `false` | [x] |
| `none` | `null` | [x] |
| `@import <file.ash>;` | `import package.module;` | [x] |
| `@import <file.ash> as x;` | `import package.Type;` (aliases are dropped, decision D1) | [x] |
| `forge(args) { ... }` (anonymous) | lambda `(args) -> expr` or `(args) -> { ... }` | [x] |
| `a and b` / `a or b` / `not a` | `a && b` / `a \|\| b` / `!a` | [x] |
| `each x in xs` (`in`) | `:` separator; `in` becomes a plain identifier | [x] |
| `e.message` | `e.getMessage()` | [x] |
| `file.NAME` (module variable) | `static` field, `ClassName.NAME` | [x] |
| builtin functions (`len`, `push`, ...) | stdlib methods (see Builtin mapping) | [x] |

## Type system

- [x] Define primitive types: `int`, `long`, `float`, `double`, `bool`, `char`, `byte`, `short`, `void`.
- [x] Define core object types: `String`, `Object`, `Array<T>`, `List<T>`, `Map<K, V>`, `Set<T>`, `Optional<T>`, `Result<T, E>`.
- [x] Require explicit parameter and return types on public methods.
- [x] Allow `var` only for local variables with obvious inferred types.
- [x] Require `final` for immutable locals and fields.
- [x] Make fields `private` by default.
- [x] Forbid public mutable fields except with explicit approval.
- [x] Define nullable types using `?`, for example `String?`.
- [x] Make non-nullable types reject possible null assignments at compile time.
- [x] Define integer, floating-point, and mixed numeric behavior precisely.
- [x] Define integer division and modulo behavior precisely.
- [x] Define generic classes, interfaces, methods, and collections.
- [x] Define type inference rules.
- [x] Define casting rules.
- [x] Define equality and hashing rules.

## Object model

- [x] Support classes.
- [x] Support interfaces.
- [x] Support abstract classes.
- [x] Support enums.
- [x] Support immutable records.
- [x] Support constructors.
- [x] Support static fields and methods.
- [x] Support instance fields and methods.
- [x] Support method overloading.
- [x] Support method overriding.
- [x] Support `@Override`.
- [x] Support interface default methods.
- [x] Support sealed types.
- [x] Support nested classes.
- [x] Support generics.
- [x] Support lambdas.
- [x] Support method references.
- [x] Support streams or an equivalent collection pipeline API.
- [x] Do not make structs, traits, mixins, or top-level functions the primary application model.

## Error handling

- [x] Use `try`, `catch`, `finally`, `throw`, and `throws`.
- [x] Define checked exceptions for recoverable I/O, parsing, network, and domain failures.
- [x] Define runtime exceptions for programming errors.
- [x] Define a base `Exception` type.
- [x] Define a base `RuntimeException` type.
- [x] Require custom exceptions to extend the appropriate base exception.
- [x] Require public methods to declare checked exceptions with `throws`.
- [x] Define stack traces. (single-frame creation site plus `Caused by:` chain, see Exceptions)
- [x] Define error messages.
- [x] Define exception chaining.
- [x] Do not use hidden error values or implicit panics.

## Standard library

- [x] Define `ash.lang`.
- [x] Define `ash.util`.
- [x] Define `ash.io`.
- [x] Define `ash.net`.
- [x] Define `ash.math`.
- [x] Define `ash.time`.
- [x] Define `ash.concurrent`.
- [x] Define `ash.json`.
- [x] Define `ash.cli`.
- [x] Define `ash.test`.
- [x] Define `ash.desktop`.
- [x] Define `ash.game`.
- [x] Define `ash.web`.
- [x] Define `System.out.println`.
- [x] Define `Console.println`.
- [x] Define `String`, `StringBuilder`, `Math`, `Object`.
- [x] Define `List<T>`, `ArrayList<T>`, `Map<K, V>`, `HashMap<K, V>`, `Set<T>`, `HashSet<T>`.
- [x] Define `Optional<T>`, `Result<T, E>`, and `Stream<T>`.
- [x] Define `Files`, `Path`, `Json`, `Http`, `Thread`, and `Executor`. (`Http` always fails and `Thread`/`Executor` run on the caller: the VM has no network or threads)
- [x] Define collection methods or stream operations: `filter`, `map`, `reduce`, `sorted`, `toList`, `collect`, `forEach`.

## Formatting rules

- [x] Use four-space indentation.
- [x] Put `{` on the same line as the declaration or statement.
- [x] Put `}` on its own line.
- [x] Use one statement per line.
- [x] Use `PascalCase` for classes, interfaces, enums, and records.
- [x] Use `lowerCamelCase` for methods, variables, fields, and parameters.
- [x] Use `UPPER_SNAKE_CASE` for constants.
- [x] Use lowercase dot-separated package names.
- [x] Use `isX()` for boolean getters.
- [x] Use `getX()` and `setX()` only when valuable.
- [x] Use `of()`, `from()`, `parse()`, and `create()` for factory methods.
- [x] Use `toString()`, `toInt()`, and similar conversion names consistently.

## Performance model

- [x] Choose the first implementation strategy: interpreter, bytecode VM, or direct tree-walking evaluator.
- [x] Choose the long-term execution strategy: AOT native compilation, JIT, or hybrid.
- [x] Define whether Ash uses tracing GC, reference counting, arenas, or a hybrid memory model.
- [x] Define primitive value representation.
- [x] Define object representation.
- [x] Define string representation.
- [x] Define array/list representation.
- [x] Define map representation.
- [x] Define function/closure representation.
- [x] Avoid unnecessary boxing.
- [x] Support value-like records.
- [x] Define concurrency: threads, executors, synchronization, and optional async/await.
- [x] Define startup-time and memory predictability requirements for games and desktop applications. (see below)

## Approved decisions (Phase 0)

| ID | Decision |
|---|---|
| D1 | `import ... as` is dropped. Imports name a type or package: `import ash.util.List;`, `import ash.util.*;`. |
| D2 | The old front end is deleted (executed 2026-10-18): `ashvm` and `kiln` accept only new Ash. Old-syntax source is rejected by the new front end with migration messages (D3). |
| D3 | Old syntax is diagnosed by context (e.g. `say` followed by an expression start at statement position), not by reserving the words. `next`, `stop`, `each`, `no`, `in`, `as`, `say` ... are ordinary identifiers in new Ash. |
| D4 | `T[]` is the surface array syntax; `Array<T>` is its canonical type name. |
| D5 | `System.out.println` is the print API. `Console.println` is removed from the spec. |
| D6 | `const` = `static final` compile-time constant. `final` alone = variable assigned once. |
| D7 | Default access: package-private for classes/methods/constructors, `private` for fields. Public mutable fields need `@PublicField`. |
| D8 | `T?` is the nullable type. `Optional<T>` for return values that may be absent. `Result<T, E>` exists only in the stdlib. Non-null `T` rejects `null` at compile time. |
| D9 | The REPL is an explicit exception: each line is wrapped in a hidden class body. It is not application syntax. |
| D10 | `} else {`, `} catch (...) {`, `} finally {` and `} while (...);` may share a line with `}` (Java style). |
| D11 | Memory model: tracing GC, reference semantics for objects. The `goals.md` ownership model is shelved. A collector now exists for `ashvm` as an opt-in build (`make GC=1`); default builds and `kiln` binaries still allocate bump/never-free. |
| D12 | One shared front end (lexer, parser, AST, type checker) lives in `hearth/front/` and is linked by both `ashvm` and `kiln`. |
| D13 | Primitive boolean type is spelled `bool`. |
| D14 | Checked exceptions are enforced (Exception subclasses not under RuntimeException must be caught or declared with `throws`). |
| D2a | **Resolved (2026-10-18).** The old-Ash-shaped text the front end lowers to is kept as a compiler-internal IR, owned by `hearth/front/src/ir` (one shared IR lexer) and passed in memory from `hearth/front/src/jdriver` to the `ashvm` compiler and the `kiln` code generator. It is not Ash source: no tool reads it from a file, there is no `@import` in it (the lowering emits one unit and the consumer calls its `start`), and no CLI, REPL, example, test or document presents it as a language. Replacing it with a binary IR driven directly by the typed AST remains possible but is not required. |
| D16 | **The standard library is optional (2026-10-18).** Only what the compiler cannot work without is built in: `ash.lang` (including `ash.lang.function`) and `ash.util.Iterator`, compiled into `libashfront.a` as C string data (`hearth/front/src/jcore`) and parsed lazily, never read from disk. Everything else (`ash.util`, `ash.io`, `ash.json`, `ash.math`, `ash.time`, `ash.cli`, `ash.test`, `ash.concurrent`, `ash.net`, `ash.web`, `ash.game`, `ash.desktop`) is the separate `ash-stdlib` package, installed with forgepack (`@ash-modules/<pkg>/src/main/ash` and `ash.libs/<pkg>/src/main/ash` are source roots) or pointed at with `ASH_PATH`. `String.join` takes `Iterable<String>` so `ash.lang` does not depend on `ash.util.List`. |
| D15 | Performance (CLAUDE.md law): no change may make the recorded baselines slower. Baselines are recorded in `spec/performance-baseline.md` before runtime changes. |

## Specification

### Keywords

Reserved: `package import class interface enum record extends implements permits sealed abstract new this super public private protected static final const native if else switch case default while for do break continue return try catch finally throw throws instanceof void true false null int long float double bool char byte short`.
Contextual: `var`, `record`, `sealed`, `permits`. `else if` is `else` followed by `if`.

### Operators (highest precedence first)

| Level | Operators | Assoc |
|---|---|---|
| 1 | `() [] . :: x++ x--` | left |
| 2 | `++x --x +x -x ! ~ (Type)cast new` | right |
| 3 | `* / %` | left |
| 4 | `+ -` | left |
| 5 | `<< >> >>>` | left |
| 6 | `< <= > >= instanceof` | left |
| 7 | `== !=` | left |
| 8 | `&` | left |
| 9 | `^` | left |
| 10 | `\|` | left |
| 11 | `&&` | left |
| 12 | `\|\|` | left |
| 13 | `?:` | right |
| 14 | `= += -= *= /= %= &= \|= ^= <<= >>=` | right |
| 15 | `->` (lambda) | right |

`&&` and `||` short-circuit. Conditions of `if`/`while`/`for`/`?:` must be `bool`; numbers, strings and `null` are not truthy.

### Numerics

- `int` is 32-bit two's complement, `long` 64-bit, `short` 16, `byte` 8, `char` 16-bit unsigned, `float` 32-bit IEEE, `double` 64-bit IEEE. Integer overflow wraps (`int` exactly with `--wrap-int`; `long` exactly with the same flag; without it `long` is limited to 2^53). Division by zero for integers throws `ArithmeticException`; for floating point it yields inf/NaN.
- Integer `/` truncates toward zero; `%` takes the sign of the dividend (`-7 % 3 == -1`).
- Mixed arithmetic widens: byte/short/char -> int -> long -> float -> double. Narrowing needs an explicit cast.
- Literals: `42` int, `42L` long, `1.5` double, `1.5f` float, `1e3` double, `0x1F`, `'a'` char, escapes `\n \t \r \\ \" \' \0 \xHH`.
- `double` prints in shortest round-trip form with a mandatory `.0` for whole values (`5.0`); the old Ash "plain integer" format is retired for doubles. `int`/`long` print as integers.
- String `+` converts the other operand with `toString()`.

### Nullability

`T` is non-null; `T?` may be `null`. Assigning a `T?` to `T`, dereferencing a `T?` without a check, or passing `null` for `T` is a compile error. `x != null` narrows `x` inside the guarded branch. `Optional<T>` is a library type, not a replacement for `T?`.

### Exceptions

`Object <- Throwable <- Exception <- RuntimeException`. Checked: `Exception` subclasses outside `RuntimeException` (`IOException`, `ParseException`, ...). Unchecked: `RuntimeException` and subclasses (`IndexOutOfBoundsException`, `KeyNotFoundException`, `ArithmeticException`, `NullPointerException`, `ClassCastException`, `IllegalArgumentException`, `IllegalStateException`). Chaining: `new X(message, cause)`, `getCause()`. Stack traces: `printStackTrace()` prints `Type: message`, then one `    at Class.method(File.ash:line)` line for the place the exception was created (the backend keeps no call stack, so there are no outer frames and no runtime cost on normal calls), then `Caused by: ...` for each cause. An uncaught exception prints that trace to stderr and exits with status 1. Runtime error text of the old VM maps as: `index out of bounds` -> `IndexOutOfBoundsException`, `key not found` -> `KeyNotFoundException`.

### Packages and compilation units

One public top-level type per file; file name equals the type name plus `.ash`. A package maps to a directory under the source root (`src/main/ash/com/acme/Main.ash` is `package com.acme;`). The program entry is the class named on the command line (or the sole class declaring `public static void main(String[] args)`). `ash.lang` is imported implicitly. Dependencies installed by forgepack (`ash.libs/`, `@ash-modules/`) are additional source roots; `ash.pkg` format is unchanged.

### Types

Primitives: `int long float double bool char byte short void`. Object types: `String Object Array<T> List<T> ArrayList<T> Map<K,V> HashMap<K,V> Set<T> HashSet<T> Optional<T> Result<T,E> Stream<T> StringBuilder Math`. Functional interfaces in `ash.lang.function`: `Function<T,R> Predicate<T> Supplier<T> Consumer<T> BiFunction<A,B,R> Runnable Comparator<T>`. Generics use erasure: type arguments are checked at compile time and not materialised at runtime; primitive type arguments are not allowed (use `Integer`, `Double` etc. boxes, which the VM represents as unboxed tagged values to avoid allocation).

### Object model details

- Enums may have fields, constructors, methods; `values()`, `ordinal()`, `name()`, `valueOf(String)`. Switch over an enum or sealed type must be exhaustive when used as an expression.
- Records: `record Point(int x, int y) { }` generate private final fields, accessors `x()`, canonical constructor (compact form allowed), `equals`, `hashCode`, `toString`.
- Interfaces: abstract methods, `default` methods, `static` methods. An interface with a single abstract method is a functional interface and is the target type for lambdas and method references.
- Switch: statement form `case X:` and expression form `case X -> expr;`. No `yield`; a block arm uses `return`-free `-> { ... }` only in statement switches; expression switches use single-expression arms.
- Lambdas capture effectively-final locals by value.
- Overloading resolves by arity then most specific applicable parameter types; ambiguity is a compile error.
- `equals`/`hashCode`: `Object` defaults to identity; strings, boxes, records and enums define value equality.
- `native` methods bind to C functions through the C ABI in `hearth/ashvm/src/builtins` and `kiln` runtime.

### Builtin mapping

| Old builtin | New API |
|---|---|
| `len(s)`, `len(a)` | `s.length()`, `a.length` / `list.size()` / `map.size()` |
| `str(x)` | `String.valueOf(x)` / `x.toString()` |
| `num(s)` | `Double.parse(s)` / `Integer.parse(s)` |
| `type(x)` | `x.getClass().getName()` |
| `sqrt abs floor` | `Math.sqrt Math.abs Math.floor` |
| `chr ord` | `(char) n`, `(int) c` |
| `input()` | `System.in.readLine()` |
| `exit(n)` | `System.exit(n)` |
| `push pop insert slice delete sort` | `list.add list.removeLast list.add(i,x) list.subList list.remove list.sort` |
| `keys values has` | `map.keys() map.values() map.containsKey` |
| `map filter reduce` | `list.stream().map/filter/reduce` |
| `split join substring indexOf contains starts_with ends_with replace repeat upper lower trim` | `split String.join substring indexOf contains startsWith endsWith replace repeat toUpperCase toLowerCase trim` |
| `read_file write_file append_file file_exists rename_file delete_file make_dir list_dir` | `Files.readString writeString appendString exists move delete createDirectory list` (throw `IOException`) |

### Formatting

Four spaces, `{` same line, `}` own line except `} else`/`} catch`/`} finally`/`} while`, one statement per line, Java naming, `isX()` boolean getters.

### Runtime representation (as implemented)

| Ash value | VM runtime value |
|---|---|
| `int long short byte float double char` | number (char is its code point; ints are exact below 2^53, see limits table) |
| `bool`, `null` | IR booleans, IR none |
| `String` | string; `Integer`, `Double`, `Boolean`... are the same unboxed values (no allocation) |
| `T[]`, `List<T>`, `Stream<T>` | array |
| `Map`, `Set` | map keyed by a string form of the key, entries `[key, value]` |
| object | map `{"__c", "__vt", "__is", fields...}`; methods are top-level functions, `__vt` is a flat key to function table, `__is` the set of supertype names |
| lambda / method reference | closure (captures by value, matching effectively-final locals) |
| record | object with synthesised accessors, `toString`, `equals`, `hashCode` |
| enum constant | object with `__name`/`__ord`, held in the class's static map |
| static state | one module-level map per class |

Startup and memory requirements for games and desktop apps: startup cost is proportional to the *reachable* classes only (tree-shaken module, no stdlib loaded unless used); there is no garbage collector yet, so a long-running program must bound its allocation (reuse objects, avoid building strings in loops) or its memory grows until exit. A collector is the prerequisite for unbounded-lifetime programs and is not part of this migration. Memory: allocation never frees until exit (D11 until a collector exists). Concurrency: no threads on this VM; `ash.concurrent.Executor` runs tasks to completion on the caller, there is no `async/await`.

### Transition and performance protocol

1. The old front end is deleted (D2). Old-syntax tests were deleted or migrated; the regression suites compare every program with its `.out` file.
2. Shared runtime tests live in `hearth/front/tests/cases/run/` with names ending `_j.ash`; project-specific ones in `<project>/tests/cases/<feature>/`.
3. Before runtime changes, `spec/performance-baseline.md` records `fib(30)` and sum-to-20M for `ashvm`; they are re-measured after each major runtime change.

## Migration phases

### Phase 0 — Specification lock

- [x] Finalize the complete keyword list.
- [x] Finalize operator precedence and associativity.
- [x] Finalize primitive and object types.
- [x] Finalize nullability rules.
- [x] Finalize exception model.
- [x] Finalize package/import model.
- [x] Finalize class/interface/enum/record model.
- [x] Finalize formatting rules.
- [x] Produce before/after examples for every old construct.

### Phase 1 — Lexer migration

- [x] Remove old keywords from the lexer.
- [x] Add all new keywords.
- [x] Add new operators and punctuation where needed.
- [x] Preserve `//` comments.
- [x] Add `/* ... */` comments.
- [x] Add `///` documentation comments.
- [x] Update lexer tests.
- [x] Ensure old Ash programs fail with clear migration errors.

### Phase 2 — Parser and AST migration

- [x] Parse package declarations.
- [x] Parse imports.
- [x] Parse classes.
- [x] Parse interfaces.
- [x] Parse enums.
- [x] Parse records.
- [x] Parse fields.
- [x] Parse constructors.
- [x] Parse methods.
- [x] Parse access modifiers.
- [x] Parse static and final modifiers.
- [x] Parse generics.
- [x] Parse `new` expressions.
- [x] Parse `this` and `super`.
- [x] Parse Java-style control flow.
- [x] Parse enhanced for loops.
- [x] Parse switch statements and/or expressions.
- [x] Parse try/catch/finally.
- [x] Parse throw and throws.
- [x] Parse lambdas.
- [x] Parse method references.
- [x] Update AST tests.

### Phase 3 — Type system

Implemented in `hearth/front/src/jtypes` (`check_expr.c`, `check_stmt.c`, `check_class.c`), driven by `ashvm check`. Covered by `hearth/front/tests/cases/types/*_j.ash`, run with `hearth/front/tests/scripts/check_types.sh`. Generics use erasure with argument-driven inference; wildcards are approximated (see `jtypes.h`).

- [x] Implement primitive type checking.
- [x] Implement class and interface type checking.
- [x] Implement generics.
- [x] Implement nullability checking.
- [x] Implement access-control checking.
- [x] Implement inheritance and override checks.
- [x] Implement interface implementation checks.
- [x] Implement method overload resolution.
- [x] Implement casting and `instanceof`.
- [x] Implement type inference for `var`.
- [x] Produce clear type errors.

### Phase 4 — Compiler/VM execution

Implemented in `hearth/front/src/jlower` as a **lowering to the internal IR** (see D2a) that `ashvm` runs and `kiln` compiles natively (no VM change, so the performance law holds by construction). The program becomes one IR unit whose `start` function is called by the consumer; objects are maps with a flat vtable, methods are top-level functions, `instanceof`/`catch` use per-class supertype tables, and native stdlib methods are `@Lower("...")` templates over the old builtins. `ashvm run file.ash` executes on `ashvm`, `kiln file.ash -o out` compiles natively. Covered by `hearth/front/tests/cases/run/*_j.ash` and `examples/`, run on both engines by `hearth/ashvm/tests/scripts/run_regression.sh` and `hearth/kiln/tests/scripts/run_regression.sh`.

- [x] Compile or interpret class declarations.
- [x] Compile or interpret constructors.
- [x] Compile or interpret methods.
- [x] Compile or interpret static members.
- [x] Compile or interpret object creation.
- [x] Compile or interpret inheritance and interfaces.
- [x] Compile or interpret exceptions.
- [x] Compile or interpret generics.
- [x] Compile or interpret lambdas and closures.
- [x] Compile or interpret collections.
- [x] Implement `public static void main(String[] args)`.
- [x] Update runtime tests.

### Phase 5 — Standard library

Source: `ash.lang` and `ash.util.Iterator` in `hearth/front/src/jcore` (core, built in); the rest in the separate `ash-stdlib` package, `src/main/ash/ash/{util,io,json,math,time,cli,test,concurrent,net,web,game,desktop}` (written in new Ash, lowered with the program; only reachable classes are emitted). Packages that need OS capabilities the old VM does not have are provided as **pure-Ash models** and say so in their doc comments: `ash.net.Url` (parsing only, no sockets), `ash.web` (Request/Response/Handler/Router, no server socket), `ash.game` (Vec2, fixed-step GameLoop, no real-time clock), `ash.desktop` (retained Widget tree rendered as text, no window), `ash.concurrent` (tasks run to completion on the caller, no threads), `ash.time.Duration` (no clock). Real sockets, windows, threads, clocks and command-line arguments need new native builtins in `ashvm` **and** `kiln` (a separate change that must be benchmarked against the performance law). `Result<T, E>` exists in `ash.util`. Not done: documentation comments on every public method.

- [x] Implement `ash.lang`.
- [x] Implement `ash.util`.
- [x] Implement `ash.io`.
- [x] Implement `ash.math`.
- [x] Implement `ash.time`. (pure `Duration`; no wall clock on this VM)
- [x] Implement `ash.concurrent`. (deterministic single-threaded `Executor`/`Future`; no real threads on this VM)
- [x] Implement `ash.json`.
- [x] Implement `ash.cli`.
- [x] Implement `ash.test`.
- [x] Add documentation for every public API. (every public stdlib type, method, constructor and constant has a `///` comment)

### Phase 6 — Examples and documentation

- [x] Hello world.
- [x] Variables and primitives.
- [x] Classes and constructors.
- [x] Interfaces and implementations.
- [x] Inheritance and abstract classes.
- [x] Enums and switch.
- [x] Records.
- [x] Generics.
- [x] Collections and streams.
- [x] Lambdas and method references.
- [x] Exceptions.
- [x] File I/O.
- [x] JSON.
- [x] CLI program.
- [x] Game program.
- [x] Backend/server program.
- [x] Desktop-style program.
- [x] Update README.
- [x] Update language reference.
- [x] Update migration guide.

## Known differences and limits of the current backend

What still differs from the spec, and why. Everything else in the Numerics, Exceptions and Switch sections is implemented and tested.

| Area | Current behaviour | Spec says | To lift it |
|---|---|---|---|
| `int` overflow | **exact Java 32-bit wrap with `--wrap-int`** (`ashvm run --wrap-int`, `kiln --wrap-int`) (`+ - * << >>>`, unary `-`, `++ --`, compound assignment, narrowing casts; `(int) double` saturates). Default is off because it costs about 2x on arithmetic-heavy loops (the performance law); for-loop counters bounded by their own condition never pay it | wraps | none needed; choose the flag |
| `long` | **exact 64-bit with `--wrap-int`**: a plain number while it fits in 2^53, an exact two-halves value beyond (`+ - * / % << >> >>> & | ^ ~`, comparisons, casts, `Long.parse`, printing, `Long.MAX_VALUE/MIN_VALUE`), checked against an independent 64-bit model. Without the flag it is a double (exact below 2^53, printed correctly) so default speed is unchanged | 64-bit | none needed; choose the flag |
| `double` / `float` | Java's shortest round-trip printing for both (`7.068577499999999`, `1.0E-4`, `3.3000002`), verified against independent oracles; `float` operations round to 24 bits after every operation; every finite value, including denormals and `Double.MAX_VALUE`, via an exact big-integer expansion for exponents outside -5..22 (`Double.MIN_VALUE` prints `4.9E-324`, as in Java) | same | none |
| Assignment, `++`, `--` inside expressions | supported with Java evaluation order (`a[k] = k++`, `x = y = 5`, `while ((n += 3) < 10)`) | allowed | none |
| `switch` | full Java semantics: fallthrough, `default` anywhere, early `break`, `continue` through a switch (simple switches stay a plain if-chain) | same | none |
| `return` through `finally` | supported (finally body is inlined); `break`/`continue` out of a try-with-finally are not specially handled | Java semantics | |
| Calls | at most 6 arguments through dynamic dispatch | unlimited | more `__vcN` helpers |
| VM limits | **256 functions** (the call opcode has a one-byte operand; `ashvm` now reports it instead of silently calling the wrong function), 128 locals per function, 256 module variables, 256 call frames, 32 nested `try`. The lowering drops unreachable code, so typical programs use a fraction of this | none | widen the operands in `ashvm` and `kiln` |
| Output | no `print` without newline, no stderr (uncaught exceptions print to stdout, exit status 1) | stderr | new builtins |
| Memory | default: allocation is never freed (D11). **`make GC=1` in `ashvm` builds a precise mark-sweep collector** (`src/vm/C/gc.c`): objects are appended to a registry as tagged pointers at their four constructors (inlined, no hashing on the allocation path; the lookup table and mark bits exist only during a collection), collection runs only at the start of opcodes that allocate (string concat, array/map literals, string indexing, closures, builtin calls; never inside a builtin callback), roots are the value stack, module globals, chunk constants and the pending raised value. The call, loop and arithmetic paths carry no added instruction. Churn program: 212 MB default vs 47 MB with the collector; identical output on every `ashvm` case with the collector running at every safe point (`tests/scripts/gc_check.sh`) and on every generated test and example (49 of 49, `ashvm` and `kiln` native). **Cost, median of 12 alternating runs, collector build / collector-free build: hashmap 1.008, fib 1.006, loop 0.998, countdown 1.007, loop_varbound 1.000** - the noise floor was measured: two collector-free builds of identical source that differ only in function alignment differ from each other by up to 0.7% (1.000, 0.993, 1.000, 0.998, 1.000), so the collector's 0.995-1.007 is inside that band; on the allocation-heavy `hashmap` it is at the top edge and it does append to a registry per allocation, so a cost of zero is not provable and it stays opt-in under the performance rule. `kiln` native binaries have no collector (its runtime is machine code on a bump/`brk` heap) | tracing GC | owner decision to enable by default, and a port to `kiln`'s runtime |
| `HashMap`/`HashSet` keys | `equals`/`hashCode`-based for any key type (collision chains; String and number keys use a fast path), size is tracked in O(1) | same | none |
| Unsupported syntax (clear parser errors) | anonymous classes, local classes, try-with-resources, static imports, `Iface.super.m()`, import aliases (D1) | | |

## Definition of done

The migration is complete when:

- [x] No old Ash keywords remain in the supported language. (both tools reject them; the old-shaped text survives only as the internal IR, see D2a)
- [x] All examples use the new enterprise Java-like syntax.
- [x] The lexer, parser, type checker, and runtime support the new model.
- [x] Every public API has explicit types.
- [x] Every recoverable I/O or parsing failure uses the defined exception model.
- [x] Every language feature has at least one example and one test. (audited by script over `examples/` and `front/tests/cases/run`)
- [x] The README documents the new language.
- [x] `MIGRATION.md` reflects the final implemented state.