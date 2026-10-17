# Ash → Enterprise Java-like Ash Migration Checklist

This document defines the target design for Ash and the ordered migration plan.

Ash must become a strictly enterprise-grade, Java-like, high-level, statically typed language.
It must always use classes, packages, explicit types, access modifiers, constructors, interfaces, enums, records, generics, and exceptions.

The old Ash syntax is deprecated. The new syntax is the only supported application-level syntax.

## Design principles

- [ ] Ash is object-oriented at the application level.
- [ ] Ash is statically typed.
- [ ] Ash is explicit and predictable.
- [ ] Ash is suitable for backend services, desktop apps, games, CLI tools, and frontend libraries.
- [ ] Ash is familiar to Java, C#, Kotlin, and TypeScript developers.
- [ ] Ash is more ergonomic than Java, but never script-like.
- [ ] Ash is designed for eventual fast native compilation.
- [ ] Every program uses a package and a class-based entry point.
- [ ] `//` comments remain supported.
- [ ] `/* ... */` and `///` comments are supported.

## Target syntax

- [ ] Use `package` declarations.
- [ ] Use `import` declarations.
- [ ] Use `public`, `private`, `protected`, and package-private access modifiers.
- [ ] Use `class`, `interface`, `enum`, `record`, `abstract`, `sealed`, and `permits`.
- [ ] Use `extends` and `implements`.
- [ ] Use `new` for object creation.
- [ ] Use `this` and `super`.
- [ ] Use `static`, `final`, and `const`.
- [ ] Use `public static void main(String[] args)` as the standard entry point.
- [ ] Use `return`, `if`, `else`, `else if`, `switch`, `case`, `default`.
- [ ] Use `while`, `for`, `do`, `break`, and `continue`.
- [ ] Use `try`, `catch`, `finally`, `throw`, and `throws`.
- [ ] Use `true`, `false`, and `null`.
- [ ] Use `instanceof`.
- [ ] Use `var` only for obvious local type inference.
- [ ] Use Java-style generics: `List<Player>`, `Map<String, Player>`, `Set<T>`.
- [ ] Use lambdas and method references.
- [ ] Use `Optional<T>` and `Result<T, E>` where appropriate.

## Keyword migration

Complete this table and implement it consistently:

| Old Ash | New Ash | Status |
|---|---|---|
| `local x = 5;` | `int x = 5;` or `var x = 5;` | [ ] |
| `say x;` | `System.out.println(x);` or `Console.println(x);` | [ ] |
| `given` | `if` | [ ] |
| `otherwise` | `else` | [ ] |
| `during` | `while` | [ ] |
| `each x in xs` | `for (Type x : xs)` | [ ] |
| `stop;` | `break;` | [ ] |
| `next;` | `continue;` | [ ] |
| `forge name(args)` | method declaration | [ ] |
| `yield value;` | `return value;` | [ ] |
| `attempt` | `try` | [ ] |
| `handle (e)` | `catch (Exception e)` | [ ] |
| `raise value;` | `throw new SomeException(...);` | [ ] |
| `yes` | `true` | [ ] |
| `no` | `false` | [ ] |
| `none` | `null` | [ ] |
| `@import <file.ash>;` | `import package.module;` | [ ] |
| `@import <file.ash> as x;` | `import package.Type;` (aliases are dropped, decision D1) | [ ] |
| `forge(args) { ... }` (anonymous) | lambda `(args) -> expr` or `(args) -> { ... }` | [ ] |
| `a and b` / `a or b` / `not a` | `a && b` / `a \|\| b` / `!a` | [ ] |
| `each x in xs` (`in`) | `:` separator; `in` becomes a plain identifier | [ ] |
| `e.message` | `e.getMessage()` | [ ] |
| `file.NAME` (module variable) | `static` field, `ClassName.NAME` | [ ] |
| builtin functions (`len`, `push`, ...) | stdlib methods (see Builtin mapping) | [ ] |

## Type system

- [ ] Define primitive types: `int`, `long`, `float`, `double`, `bool`, `char`, `byte`, `short`, `void`.
- [ ] Define core object types: `String`, `Object`, `Array<T>`, `List<T>`, `Map<K, V>`, `Set<T>`, `Optional<T>`, `Result<T, E>`.
- [ ] Require explicit parameter and return types on public methods.
- [ ] Allow `var` only for local variables with obvious inferred types.
- [ ] Require `final` for immutable locals and fields.
- [ ] Make fields `private` by default.
- [ ] Forbid public mutable fields except with explicit approval.
- [ ] Define nullable types using `?`, for example `String?`.
- [ ] Make non-nullable types reject possible null assignments at compile time.
- [ ] Define integer, floating-point, and mixed numeric behavior precisely.
- [ ] Define integer division and modulo behavior precisely.
- [ ] Define generic classes, interfaces, methods, and collections.
- [ ] Define type inference rules.
- [ ] Define casting rules.
- [ ] Define equality and hashing rules.

## Object model

- [ ] Support classes.
- [ ] Support interfaces.
- [ ] Support abstract classes.
- [ ] Support enums.
- [ ] Support immutable records.
- [ ] Support constructors.
- [ ] Support static fields and methods.
- [ ] Support instance fields and methods.
- [ ] Support method overloading.
- [ ] Support method overriding.
- [ ] Support `@Override`.
- [ ] Support interface default methods.
- [ ] Support sealed types.
- [ ] Support nested classes.
- [ ] Support generics.
- [ ] Support lambdas.
- [ ] Support method references.
- [ ] Support streams or an equivalent collection pipeline API.
- [ ] Do not make structs, traits, mixins, or top-level functions the primary application model.

## Error handling

- [ ] Use `try`, `catch`, `finally`, `throw`, and `throws`.
- [ ] Define checked exceptions for recoverable I/O, parsing, network, and domain failures.
- [ ] Define runtime exceptions for programming errors.
- [ ] Define a base `Exception` type.
- [ ] Define a base `RuntimeException` type.
- [ ] Require custom exceptions to extend the appropriate base exception.
- [ ] Require public methods to declare checked exceptions with `throws`.
- [ ] Define stack traces.
- [ ] Define error messages.
- [ ] Define exception chaining.
- [ ] Do not use hidden error values or implicit panics.

## Standard library

- [ ] Define `ash.lang`.
- [ ] Define `ash.util`.
- [ ] Define `ash.io`.
- [ ] Define `ash.net`.
- [ ] Define `ash.math`.
- [ ] Define `ash.time`.
- [ ] Define `ash.concurrent`.
- [ ] Define `ash.json`.
- [ ] Define `ash.cli`.
- [ ] Define `ash.test`.
- [ ] Define `ash.desktop`.
- [ ] Define `ash.game`.
- [ ] Define `ash.web`.
- [ ] Define `System.out.println`.
- [ ] Define `Console.println`.
- [ ] Define `String`, `StringBuilder`, `Math`, `Object`.
- [ ] Define `List<T>`, `ArrayList<T>`, `Map<K, V>`, `HashMap<K, V>`, `Set<T>`, `HashSet<T>`.
- [ ] Define `Optional<T>`, `Result<T, E>`, and `Stream<T>`.
- [ ] Define `Files`, `Path`, `Json`, `Http`, `Thread`, and `Executor`.
- [ ] Define collection methods or stream operations: `filter`, `map`, `reduce`, `sorted`, `toList`, `collect`, `forEach`.

## Formatting rules

- [ ] Use four-space indentation.
- [ ] Put `{` on the same line as the declaration or statement.
- [ ] Put `}` on its own line.
- [ ] Use one statement per line.
- [ ] Use `PascalCase` for classes, interfaces, enums, and records.
- [ ] Use `lowerCamelCase` for methods, variables, fields, and parameters.
- [ ] Use `UPPER_SNAKE_CASE` for constants.
- [ ] Use lowercase dot-separated package names.
- [ ] Use `isX()` for boolean getters.
- [ ] Use `getX()` and `setX()` only when valuable.
- [ ] Use `of()`, `from()`, `parse()`, and `create()` for factory methods.
- [ ] Use `toString()`, `toInt()`, and similar conversion names consistently.

## Performance model

- [ ] Choose the first implementation strategy: interpreter, bytecode VM, or direct tree-walking evaluator.
- [ ] Choose the long-term execution strategy: AOT native compilation, JIT, or hybrid.
- [ ] Define whether Ash uses tracing GC, reference counting, arenas, or a hybrid memory model.
- [ ] Define primitive value representation.
- [ ] Define object representation.
- [ ] Define string representation.
- [ ] Define array/list representation.
- [ ] Define map representation.
- [ ] Define function/closure representation.
- [ ] Avoid unnecessary boxing.
- [ ] Support value-like records.
- [ ] Define concurrency: threads, executors, synchronization, and optional async/await.
- [ ] Define startup-time and memory predictability requirements for games and desktop applications.

## Approved decisions (Phase 0)

| ID | Decision |
|---|---|
| D1 | `import ... as` is dropped. Imports name a type or package: `import ash.util.List;`, `import ash.util.*;`. |
| D2 | The old front end stays frozen and runnable (`.ash` files without a `package`/`class` header go through it) until the new front end reaches parity; then it is deleted. New front end is selected per file by syntax detection (a file whose first token is `package`, `import`, `public`, `class`, `interface`, `enum`, `record`, `abstract`, `final` or `sealed` is new Ash). |
| D3 | Old syntax is diagnosed by context (e.g. `say` followed by an expression start at statement position), not by reserving the words. `next`, `stop`, `each`, `no`, `in`, `as`, `say` ... are ordinary identifiers in new Ash. |
| D4 | `T[]` is the surface array syntax; `Array<T>` is its canonical type name. |
| D5 | `System.out.println` is the print API. `Console.println` is removed from the spec. |
| D6 | `const` = `static final` compile-time constant. `final` alone = variable assigned once. |
| D7 | Default access: package-private for classes/methods/constructors, `private` for fields. Public mutable fields need `@PublicField`. |
| D8 | `T?` is the nullable type. `Optional<T>` for return values that may be absent. `Result<T, E>` exists only in the stdlib. Non-null `T` rejects `null` at compile time. |
| D9 | The REPL is an explicit exception: each line is wrapped in a hidden class body. It is not application syntax. |
| D10 | `} else {`, `} catch (...) {`, `} finally {` and `} while (...);` may share a line with `}` (Java style). |
| D11 | Memory model: tracing GC, reference semantics for objects. The `goals.md` ownership model is shelved. Until a collector lands, allocation is bump/never-free, as today. |
| D12 | One shared front end (lexer, parser, AST, type checker) lives in `front/` and is linked by both `ashvm` and `kiln`. |
| D13 | Primitive boolean type is spelled `bool`. |
| D14 | Checked exceptions are enforced (Exception subclasses not under RuntimeException must be caught or declared with `throws`). |
| D15 | Performance (CLAUDE.md law): no change may make the recorded baselines slower. Baselines are recorded in `docs/performance-baseline.md` before runtime changes. |

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

- `int` is 32-bit two's complement, `long` 64-bit, `short` 16, `byte` 8, `char` 16-bit unsigned, `float` 32-bit IEEE, `double` 64-bit IEEE. Integer overflow wraps. Division by zero for integers throws `ArithmeticException`; for floating point it yields inf/NaN.
- Integer `/` truncates toward zero; `%` takes the sign of the dividend (`-7 % 3 == -1`).
- Mixed arithmetic widens: byte/short/char -> int -> long -> float -> double. Narrowing needs an explicit cast.
- Literals: `42` int, `42L` long, `1.5` double, `1.5f` float, `1e3` double, `0x1F`, `'a'` char, escapes `\n \t \r \\ \" \' \0 \xHH`.
- `double` prints in shortest round-trip form with a mandatory `.0` for whole values (`5.0`); the old Ash "plain integer" format is retired for doubles. `int`/`long` print as integers.
- String `+` converts the other operand with `toString()`.

### Nullability

`T` is non-null; `T?` may be `null`. Assigning a `T?` to `T`, dereferencing a `T?` without a check, or passing `null` for `T` is a compile error. `x != null` narrows `x` inside the guarded branch. `Optional<T>` is a library type, not a replacement for `T?`.

### Exceptions

`Object <- Throwable <- Exception <- RuntimeException`. Checked: `Exception` subclasses outside `RuntimeException` (`IOException`, `ParseException`, ...). Unchecked: `RuntimeException` and subclasses (`IndexOutOfBoundsException`, `KeyNotFoundException`, `ArithmeticException`, `NullPointerException`, `ClassCastException`, `IllegalArgumentException`, `IllegalStateException`). Chaining: `new X(message, cause)`, `getCause()`. Stack traces: `printStackTrace()` prints `Type: message` then `    at Class.method(File.ash:line)` per frame. An uncaught exception prints that trace to stderr and exits with status 1. Runtime error text of the old VM maps as: `index out of bounds` -> `IndexOutOfBoundsException`, `key not found` -> `KeyNotFoundException`.

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
- `native` methods bind to C functions through the C ABI in `ashvm/src/builtins` and `kiln` runtime.

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

### Transition and performance protocol

1. Old front end remains runnable (D2); the regression suites for old tests must stay byte-identical until the old path is deleted.
2. New tests live beside old ones in `tests/cases/<feature>/` with names ending `_j.ash`.
3. Before runtime changes, `docs/performance-baseline.md` records `fib(30)` and sum-to-20M for `ashvm`; they are re-measured after each major runtime change.

## Migration phases

### Phase 0 — Specification lock

- [ ] Finalize the complete keyword list.
- [ ] Finalize operator precedence and associativity.
- [ ] Finalize primitive and object types.
- [ ] Finalize nullability rules.
- [ ] Finalize exception model.
- [ ] Finalize package/import model.
- [ ] Finalize class/interface/enum/record model.
- [ ] Finalize formatting rules.
- [ ] Produce before/after examples for every old construct.

### Phase 1 — Lexer migration

- [ ] Remove old keywords from the lexer.
- [ ] Add all new keywords.
- [ ] Add new operators and punctuation where needed.
- [ ] Preserve `//` comments.
- [ ] Add `/* ... */` comments.
- [ ] Add `///` documentation comments.
- [ ] Update lexer tests.
- [ ] Ensure old Ash programs fail with clear migration errors.

### Phase 2 — Parser and AST migration

- [ ] Parse package declarations.
- [ ] Parse imports.
- [ ] Parse classes.
- [ ] Parse interfaces.
- [ ] Parse enums.
- [ ] Parse records.
- [ ] Parse fields.
- [ ] Parse constructors.
- [ ] Parse methods.
- [ ] Parse access modifiers.
- [ ] Parse static and final modifiers.
- [ ] Parse generics.
- [ ] Parse `new` expressions.
- [ ] Parse `this` and `super`.
- [ ] Parse Java-style control flow.
- [ ] Parse enhanced for loops.
- [ ] Parse switch statements and/or expressions.
- [ ] Parse try/catch/finally.
- [ ] Parse throw and throws.
- [ ] Parse lambdas.
- [ ] Parse method references.
- [ ] Update AST tests.

### Phase 3 — Type system

- [ ] Implement primitive type checking.
- [ ] Implement class and interface type checking.
- [ ] Implement generics.
- [ ] Implement nullability checking.
- [ ] Implement access-control checking.
- [ ] Implement inheritance and override checks.
- [ ] Implement interface implementation checks.
- [ ] Implement method overload resolution.
- [ ] Implement casting and `instanceof`.
- [ ] Implement type inference for `var`.
- [ ] Produce clear type errors.

### Phase 4 — Compiler/VM execution

- [ ] Compile or interpret class declarations.
- [ ] Compile or interpret constructors.
- [ ] Compile or interpret methods.
- [ ] Compile or interpret static members.
- [ ] Compile or interpret object creation.
- [ ] Compile or interpret inheritance and interfaces.
- [ ] Compile or interpret exceptions.
- [ ] Compile or interpret generics.
- [ ] Compile or interpret lambdas and closures.
- [ ] Compile or interpret collections.
- [ ] Implement `public static void main(String[] args)`.
- [ ] Update runtime tests.

### Phase 5 — Standard library

- [ ] Implement `ash.lang`.
- [ ] Implement `ash.util`.
- [ ] Implement `ash.io`.
- [ ] Implement `ash.math`.
- [ ] Implement `ash.time`.
- [ ] Implement `ash.concurrent`.
- [ ] Implement `ash.json`.
- [ ] Implement `ash.cli`.
- [ ] Implement `ash.test`.
- [ ] Add documentation for every public API.

### Phase 6 — Examples and documentation

- [ ] Hello world.
- [ ] Variables and primitives.
- [ ] Classes and constructors.
- [ ] Interfaces and implementations.
- [ ] Inheritance and abstract classes.
- [ ] Enums and switch.
- [ ] Records.
- [ ] Generics.
- [ ] Collections and streams.
- [ ] Lambdas and method references.
- [ ] Exceptions.
- [ ] File I/O.
- [ ] JSON.
- [ ] CLI program.
- [ ] Game program.
- [ ] Backend/server program.
- [ ] Desktop-style program.
- [ ] Update README.
- [ ] Update language reference.
- [ ] Update migration guide.

## Definition of done

The migration is complete when:

- [ ] No old Ash keywords remain in the supported language.
- [ ] All examples use the new enterprise Java-like syntax.
- [ ] The lexer, parser, type checker, and runtime support the new model.
- [ ] Every public API has explicit types.
- [ ] Every recoverable I/O or parsing failure uses the defined exception model.
- [ ] Every language feature has at least one example and one test.
- [ ] The README documents the new language.
- [ ] `MIGRATION.md` reflects the final implemented state.