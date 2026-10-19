# Memory safety

Ash checks your program for memory errors when it compiles it, and frees memory for you when it runs it. There is
no garbage collector in the default build and nothing to write: you keep writing Java-like code, and the compiler
refuses a program only when it can prove that something goes wrong.

This page lists every rule, with a short example and the error you get. All messages below are copied from the
compiler.

- [The setting](#the-setting)
- [What is checked and what is not](#what-is-checked-and-what-is-not)
- [Ownership: moves](#ownership-moves)
- [Borrows](#borrows)
- [Ranges: indexes, overflow, division, array sizes](#ranges-indexes-overflow-division-array-sizes)
- [Lists and Strings](#lists-and-strings)
- [Initialisation](#initialisation)
- [Recursion without a way out](#recursion-without-a-way-out)
- [For-each over a list](#for-each-over-a-list)
- [Freeing](#freeing)
- [Loop regions (kiln)](#loop-regions-kiln)
- [`System.free`](#systemfree)
- [Switches and environment variables](#switches-and-environment-variables)
- [Error codes](#error-codes)
- [Checking the checks](#checking-the-checks)

## The setting

```
memory-safety: on
```

in the header of the project's `ash.pkg` (before the first `[section]`). `on` is the default, also when there is no
`ash.pkg` at all (the directory of the file you compile is then the project). `off` turns every check and every
free below off and generates exactly the code Ash generated before these checks existed.

- Only the project's own sources are checked. The standard library and dependencies in `@ash-modules` are not,
  because you cannot change them.
- Anything that is not `off` at the start of the value counts as `on` (`false`, `no`, `OFF` are `on`). `apm check`
  reports a value other than `on` / `off` as a problem, a misspelt key (`memory_safety`) or a `memory-safety` line
  placed under a `[section]` as a warning, and `memory-safety: off` as a warning.
- The checks run in `ashvm run`, `ashvm check` and `kiln`; `ashvm check file.ash` only checks.

## What is checked and what is not

One principle holds for every check: **only failures that are certain are errors.** If the compiler can only say
"this might go wrong", it stays quiet and the run-time check (an exception, as in Java) does its job. A valid
program is never rejected by a range or initialisation check; the ownership checks are stricter by design (see
below). Values the analysis cannot follow are unknown and never reported: parameters, fields, results of calls,
anything inside a lambda. Code inside a `try` that has a `catch` is not reported either, because programs provoke
exceptions on purpose there.

## Ownership: moves

A local of an owning type (a class, a String or an array; not primitives, boxes or enums) is **moved** when it is
stored somewhere: `b = a`, `T b = a`, `this.f = a`, `xs[i] = a`, `{a}`, or when it is passed to project code that
keeps it. Using it afterwards is an error until it gets a new value.

```java
static int[] keep;
static void store(int[] p) { keep = p; }
static int sum(int[] p) { int s = 0; for (int v : p) s += v; return s; }

int[] a = {1, 2, 3};
System.out.println(sum(a) + sum(a));   // fine: sum only reads its parameter
store(a);                              // a moves into the static field
System.out.println(a[0]);              // error
```

```
error[E0101]: use of moved value 'a': it was moved earlier by an assignment or by passing it to a call; lend it with &a, clone it, or assign a new value first
```

- A call only moves its argument when the callee can keep it. The compiler works that out for project methods
  whose target is fixed (`static`, `private`, `final`, constructors, classes nothing extends): a parameter that the
  body only reads (indexing, `.length`, operands, String methods that return primitives, printing, passing it on
  to another read-only parameter) is borrowed, so `sum(a)` twice is fine. Library and native calls only look at
  their arguments and never move.
- Moves follow `if`, `switch`, `try`, loops (a move that survives to the back edge is caught), `return`, `throw`,
  `break` and `continue`. A move in one branch counts after the `if`.
- A variable declared inside a loop body is a new variable on every iteration, and so are catch variables,
  `instanceof` bindings and switch type-pattern bindings; a variable declared outside the loop and moved inside it
  is an error on the second iteration.
- Assigning a new value brings a moved variable back to life.

## Borrows

`&x` lends a local (shared), `&mut x` lends it exclusively. They are written only as call arguments and cost nothing
at run time (the call receives the variable itself).

- Within one call an exclusive borrow excludes every other use of the same variable: `f(&mut x, &x)`,
  `f(&mut x, x)` and `x.m(&mut x)` are errors.
- `&x` anywhere else is an error: `a borrow ('&xs') can only be passed as a call argument; it cannot be stored,
  returned or assigned, so pass 'xs' by move or clone it`.
- A loan cannot be kept (Rust's rule that a borrow cannot outlive the call). Lending to a project method that
  stores, returns or passes on that parameter is an error:

```java
store(&a);
```

```
error[E0102]: cannot lend 'a' with '&': 'store' keeps this parameter (stores, returns or passes it on); pass 'a' by move or clone it
```

  Calls whose target is not fixed (overridable or library code) accept a loan, and a lent variable is never freed
  by the compiler, since the callee could keep it.

## Ranges: indexes, overflow, division, array sizes

The compiler tracks the possible values of `int` / `long` locals and the lengths of arrays, through loops (to a
fixpoint, with widening) and conditions (`if (i < a.length)` narrows `i` in the branch; code behind an impossible
condition is dead and quiet). It rejects:

| Rule | Example | Error (start) |
| --- | --- | --- |
| an index that is always negative or always past the end | `int[] a = new int[4]; int i = 4; a[i] = 1;` | `array index is always out of bounds here: the index is 4 and the array length is 4` |
| an `int` `+ - * ++ -- += -= *=` that always leaves the 32-bit range (not under `--wrap-int`) | `int x = 2000000000; x + x` | `arithmetic overflow: this expression is always outside the range of int here (4000000000)` |
| a divisor that is always 0 (a literal `/ 0` stays a run-time exception) | `int d = 0; 10 / d` | `division by zero: the divisor is always 0 here` |
| a negative array size | `new int[n]` with `n` always negative | `negative array size: the length is always negative here (-1)` |

Every message ends with a hint (for the first: `valid indexes are 0 to length - 1, so fix the index or the array
size, or check the index first`) and the caret points at the token to change: the index, the divisor, the length.

Loops are judged precisely: `for (int i = 10; i < 20; i++) a[i]` with a 4-element `a` reports `the index is 10 to
19`. The overflow error says `use long, or --wrap-int for wrap-around`.

**Constants.** A constant `int` expression outside the 32-bit range (`2147483647 + 1`, `Integer.MAX_VALUE + 1`,
`final int n = 100000; n * n`) and a constant array index that is negative or past the length of a literal /
`final` array are errors as well (`arithmetic overflow: constant expression is outside the range of int; use an
'L' suffix for long`).

**Array lengths** are known for `new T[n]` with a known `n`, array literals, copies (`b = a`), `c ? x : y` (the
hull of both arms), `static final int` sizes, and array fields that every assignment in the whole project gives a
fixed size (`private int[] cells; C() { cells = new int[8]; }` makes `cells[8]` an error). One assignment from a
parameter, a call or `null` makes the field's length unknown.

**`switch`** narrows the selector inside a case: in `case 3:` / `case 5, 6 ->` an `int` / `char` selector local is
exactly the labels' range.

## Lists and Strings

- `l.get(i)`, `l.set(i, v)`, `l.remove(i)`: an `i` that is always negative or always at or past the known size is an
  error (`list index is always out of bounds here: the index is 1 and the list size is 1`). The size is followed for
  a `List<T>` / `ArrayList<T>` local that starts as `new ArrayList<>()` through `add`, `remove`, `removeLast`,
  branches and loops, and `l.size()` evaluates to it. A list is only followed while every use is `size`, `isEmpty`,
  `get`, `set`, `contains`, `add`, `remove` or `removeLast` on it, outside lambdas, and every assignment is another
  fresh `new ArrayList<>()`. Passing it to a call, a for-each over it, `return l`, `sort`, `subList`, `stream` or any
  capture stops the checks for that local.
- `s.charAt(i)`: an `i` that is always negative or always at or past the known length is an error (`string index is
  always out of bounds here: the index is 3 and the string length is 3`). The length is known for ASCII literals,
  concatenations of known Strings and String locals holding such a value; `s.length()` evaluates to it.
  `substring` is not checked because it does not throw.

## Initialisation

A local that no path assigns before it is read is an error:

```java
int x;
System.out.println(x);
```

```
error[E0301]: variable 'x' is used before it is assigned: no path assigns it before this use; give it a value where it is declared, or assign it first
```

- Only a use that *no* path can have assigned is reported: `if (c) x = 1; use(x);` is legal (the program may know
  that `c` holds).
- The first pass through a loop is judged on its straight-line part: `int total; for (..) { print(total); total =
  i; }` is `variable 'total' is used before it is assigned: the first pass through the loop reaches this use before
  any assignment`. A use behind a condition, `&&` / `||`, `?:`, a lambda, a `try` with a catch, an inner loop or
  after a `continue` is not reported, because an earlier iteration may have assigned it.
- `&mut x` counts as an assignment (the callee fills it in). A `try` with a `catch` and lambda bodies stay quiet.

## Recursion without a way out

A `static`, `private` or `final` method that calls itself on every path before any branch, loop or `return` can never
finish:

```java
static int f(int n) { return f(n + 1); }
```

```
error[E0401]: infinite recursion: 'f' calls itself on every path before any condition, so it can never return; add a base case (an `if` that returns before this call)
```

Overridable instance methods are not judged (a subclass may end the recursion). The caret is on the recursive call.

## For-each over a list

A for-each over an `ArrayList` / `List` follows Java's iterator, in `ashvm` and `kiln` alike: the loop ends when the
position reaches the size, and a size that changed since the loop began throws
`ash.util.ConcurrentModificationException` when the next element is asked for. `set` is fine; adding throws at the
next step; removing the second-to-last element just ends the loop; removing the last throws; `break` right after a
change is fine. The exception carries the message `the list changed size while a for-each was walking it; loop over a
copy, or collect the changes and apply them after the loop`.

```java
for (int x : xs) xs.add(x);   // throws ConcurrentModificationException at the second step
```

The check costs run-time speed, so the compiler only emits it when the loop body could change a list's size. It is
left out (the plain index loop) when the body's calls are all harmless: String methods, `Math`, `println` of values,
`get` / `size` / `isEmpty` / `set` on a list, and calls to project code whose target is fixed (`static`, `private` or
`final`) and whose own body is harmless in the same way, transitively (a recursion is judged as a whole). It is also
left out for lists nobody else can reference: a list local that only ever holds fresh `new ArrayList<>()`s and is
used only as the receiver of `get` / `set` / `size` / `isEmpty` / `contains` / `add` / `remove` / `removeLast`, as the
iterable of a for-each, or returned. Adding to such a list while walking another list needs no check, and walking such
a list needs none either, whatever the body calls, unless the body adds to or removes from that same local. Anything
else (an alias, a list passed on, a field, a lambda, a view such as `subList`) keeps the check. Arrays have
no check (their size cannot change). Not mirrored: Java counts modifications and Ash compares sizes, so an `add`
followed by a `remove` that leaves the size equal is not detected.

## Freeing

With `memory-safety: on` memory is given back without a collector. A local that is the **sole owner** of a fresh
value and is only read is freed at the end of its scope, before `return` / `break` / `continue`, and before it is
reassigned. Concatenation temporaries are freed as soon as they are used.

- **Which values.** Strings built by a non-constant `+`, arrays (`new T[n]`, literals), `ArrayList`s,
  `HashMap` / `HashSet`s, and plain objects (`new T(..)` where `this` never escapes, or the fresh result of a project
  method that returns one). `ashvm` frees all of them; `kiln` frees Strings, one-dimensional arrays, objects, maps
  and sets (Linux and Windows static builds). `new T[a][b]` frees its rows with it.
- **What "only read" means.** Index, `.length`, operands, String methods returning primitives, print arguments,
  for-each, and for lists / maps the methods that cannot hand out the container (`size`, `get`, `set`, `add`,
  `remove`, `contains`, `put`, `containsKey`, `keys`, `values`, ...). Returning it, storing it, aliasing it, passing
  it to a call that keeps it, capturing it in a lambda or `sort` / `subList` / `stream` keep it alive.
- **Owned fields.** A String or array field of a plain class that is only ever assigned fresh values and only read in
  harmless ways is freed with its object; reassigning it frees the old value.
- **Elements.** A `List<String>` / `String[]` whose elements are all fresh concatenations and are only read in
  harmless ways frees those Strings with it.
- **The rule that matters:** everything the compiler cannot prove unshared is left alone. A leak is possible, a
  use-after-free is not.

A loop building millions of short-lived values runs in a few megabytes (`ashvm/tests/scripts/memory_check.sh` and
`kiln/tests/scripts/memory_check.sh` measure it). `--link=shared` builds of `kiln` keep the old allocator and free
nothing.

## Loop regions (kiln)

`kiln` allocates from a bump heap, so for a loop whose body keeps nothing it allocates past the iteration it saves
the heap pointer at the start of every iteration and moves it back at the end: everything the iteration allocated
is given back at once. This is only done where it is provably safe (`front/src/jown/C/region.c`), Linux and
Windows static builds, `memory-safety: on`. A loop body qualifies when:

- it calls no project code, except methods that provably keep nothing and return a primitive (`static`, `private`
  or `final`, parameters primitives and Strings, no recursion, themselves satisfying these rules); the calls that
  remain are String methods that read, `Math`, `println` of values, and the `ArrayList` / `HashMap` /
  `HashSet` methods on a collection **declared in the body** with String or number elements;
- it has no project constructor, lambda, `switch` or `throw`; `try` / `catch` / `finally` are allowed when nothing
  inside can raise a project exception;
- every reference it stores goes to a variable declared in the body (a String, or an array / list that starts fresh),
  never to an outer variable, a field, an element of an outer array or an outer list; primitives may leave freely;
- a for-each is allowed over an array or `ArrayList` (the loop variable of a reference type is only read).

A loop whose only allocations are Strings and arrays that a sole-owner local already gives back every iteration is not
made a region: it would gain nothing (the blocks are recycled while they are hot) and cost the bookkeeping. The body
block of a region also does not free its own locals one by one before the rewind. `break` and `return` skip the
rewind, which only leaves the memory allocated. The loop condition and update run
outside the region. A rewind also empties the free lists (a block freed inside the region would otherwise be handed
out again over newer data): a leak, never an overlap. Everything else is an ordinary loop.

```java
for (int i = 0; i < 2000000; i++) {
    String s = "item-" + i;     // dies with the iteration: a region
    last = "kept-" + i;         // stored in a static: not a region
}
```

`ASH_REGION_TRACE=2 kiln Reg.ash -o reg` prints `region loop: Reg.ash:5` for each region and `no region: Reg.ash:12
(stopped at Reg.ash:13 kind 54)` for loops that are not, with the node that stopped them.

## `System.free`

With `memory-safety: off`, `System.free(x)` frees a String, array or object yourself in `ashvm` (`kiln` ignores it,
since it cannot know where an arbitrary String came from). With safety on it is an error:

```
error[E0105]: System.free is only available with 'memory-safety: off' in ash.pkg: with it on, memory is freed for you
```

## Switches and environment variables

| Switch | Where | Effect |
| --- | --- | --- |
| `memory-safety: off` | `ash.pkg` header | no checks, no automatic frees, `System.free` allowed |
| `--wrap-int` | `ashvm run`, `kiln` | Java's 32-bit wrap-around for `int` arithmetic; the overflow checks (constant and range) are off, because wrapping is then the defined behaviour (costs about 2x on arithmetic-heavy loops) |
| `--link=shared` | `kiln` | the old allocator: no frees, no regions |
| `KILN_NO_REGIONS=1` | environment, when running `kiln` | no loop regions (frees by ownership still happen) |
| `KILN_REGION_POISON=1` | environment, when running `kiln` | every rewind first overwrites the reclaimed bytes with `0xDD`, so a value used after its iteration shows up as garbage; for testing the region analysis |
| `ASH_REGION_ALL=1` | environment, when running `kiln` | also make regions of loops that gain nothing (only Strings / arrays a sole owner frees); the region tests use it so that those loops keep being tested with poison |
| `ASH_FOREACH_CHECK_ALL=1` | environment, when running `ashvm` or `kiln` | every for-each over a list keeps its size check (the fuzzers compare against it to see that no check was left out wrongly) |
| `ASH_REGION_TRACE=1` / `2` | environment, when running `kiln` or `ashvm` | `1` lists the region loops (and, in `ashvm` too, says for each for-each over a list whether it keeps its size check), `2` also says why a loop is not one |
| `ASH_PATH` | environment | where the standard library lives (`.../src/main/ash`) |
| `make GC=1` | building `ashvm` | an opt-in collector, independent of the above; `ASH_GC_STATS` (any value) prints its counters at exit and `ASH_GC_STRESS=1` collects at every opportunity |

Compile-time analysis cost: nested loops are walked again for every pass of the loop around them. A loop that is
only searching for the fixpoint of an outer loop stops when its state is stable, and after 2000 loop passes in one
method it does a single pass and gives up its ranges (the moved-variable state stays exact), so the cost stays
linear; only range errors are lost in nests that deep.

## Error codes

Every memory-safety error carries a stable code, shown in brackets after `error` (`error[E0101]: use of moved value
'a': ...`) and as `"code"` in the `--json` output. The text after the colon may be reworded; the code is the name of
the rule. `ashvm explain E0101` (or `kiln --explain E0101`) prints the entry below; `ashvm explain` alone lists the
codes.

### E0101: use of moved value

A local of an owning type (a class, a String or an array) was stored somewhere or passed to project code that keeps it,
and then used again. The error points at the use; the note under it points at the move.

```java
static int[] keep;
static void store(int[] p) { keep = p; }

int[] a = {1, 2, 3};
store(a);                      // a moves into the static field
System.out.println(a[0]);      // E0101
```

What to do: use the value before the move (reorder the statements), use the variable or field it was moved into
instead, give the variable a new value (`a = new int[3];`) before using it again, or, when the callee is a call whose
target is not fixed (an interface or an overridable method), lend the value with `&a`. A method the compiler can see
keeps its parameter cannot be lent to (see E0102). In a loop, a variable moved in one pass is moved in the next: declare
it inside the loop or assign it again.

### E0102: cannot lend a variable to a call that keeps it

`&x` lends a variable for the duration of one call. A project method whose target is fixed (`static`, `private`,
`final`, a constructor, a class nothing extends) and that stores, returns or passes on that parameter would outlive the
loan, so the compiler refuses it.

```java
store(&a);      // E0102: 'store' keeps this parameter
```

What to do: pass the value itself (`store(a)`, and stop using `a`), or change the method so that it only reads its
parameter.

### E0103: conflicting borrows in one call

Within one call an exclusive borrow (`&mut x`) excludes every other use of the same variable, as in Rust:
`f(&mut x, &x)`, `f(&mut x, x)` and `x.m(&mut x)` are errors, and so is a `&x` next to a mutable use.

What to do: pass the variable once, or make two calls.

### E0104: a borrow outside a call argument

`&x` and `&mut x` are written only as call arguments. They cannot be stored, returned or assigned.

```java
int[] ys = &xs;      // E0104
```

What to do: pass `xs` by move, or use it where it is.

### E0105: System.free with memory-safety on

`System.free(x)` frees a value by hand and is only available with `memory-safety: off` in `ash.pkg`. With the checks on,
memory is freed for you.

What to do: delete the call, or turn the setting off for the project.

### E0201: array index always negative

The index of an array access is negative on every path that reaches it, so the access always throws.

What to do: fix the index, or check it before the access.

### E0202: array index always out of bounds

The index is at or past the length of the array on every path that reaches it.

```java
int[] a = new int[4];
int i = 4;
a[i] = 1;           // E0202: the index is 4 and the array length is 4
```

What to do: fix the index or the array size, or check the index first. A loop that merely might run past the end is not
reported; only a certain failure is.

### E0203: list index always negative

`l.get(i)`, `l.set(i, v)` or `l.remove(i)` with an index that is negative on every path.

What to do: fix the index, or check it before the call.

### E0204: list index always out of bounds

The index is at or past the size the compiler knows the list has (it follows `add`, `remove` and `removeLast` on a list
that only this code references).

What to do: fix the index, add the element first, or check `size()` before the call.

### E0205: string index always negative

`s.charAt(i)` with an index that is negative on every path.

What to do: fix the index, or check it before the call.

### E0206: string index always out of bounds

`s.charAt(i)` with an index at or past the length the compiler knows the string has (ASCII literals, concatenations of
known strings).

What to do: fix the index, or check `length()` first.

### E0207: arithmetic overflow

An `int` `+ - * ++ -- += -= *=` whose result is outside the 32-bit range on every path (not reported with `--wrap-int`).

```java
int x = 2000000000;
int y = x + x;      // E0207
```

What to do: use `long`, or compile with `--wrap-int` for Java's wrap-around.

### E0208: division by zero

The divisor of `/` or `%` is 0 on every path (a literal `/ 0` stays a run-time exception).

What to do: check the divisor before dividing, or use a different one.

### E0209: negative array size

`new int[n]` where `n` is negative on every path.

What to do: an array needs a length of 0 or more.

### E0210: constant overflow

A constant `int` expression outside the 32-bit range, such as `2147483647 + 1` or `Integer.MAX_VALUE + 1`
(not reported with `--wrap-int`).

What to do: use an `L` suffix to compute in `long`.

### E0211: constant array index is negative

A constant index into a literal or `final` array is negative.

What to do: array indexes start at 0.

### E0212: constant array index out of bounds

A constant index into a literal or `final` array is at or past its length.

What to do: fix the index or the array.

### E0301: variable used before it is assigned

A local that no path assigns before it is read. `if (c) x = 1; use(x);` is legal (the program may know that `c` holds);
only a use that no path can have assigned is reported, and so is a use in the first pass of a loop that reads the
variable before the body assigns it.

```java
int x;
System.out.println(x);      // E0301
```

What to do: give the variable a value where it is declared, or assign it first.

### E0401: infinite recursion

A `static`, `private` or `final` method that calls itself on every path before any branch, loop or `return` can never
finish.

```java
static int f(int n) { return f(n + 1); }      // E0401
```

What to do: add a base case, an `if` that returns before the recursive call.

## Checking the checks

```sh
hearth/ashvm/tests/scripts/run_regression.sh         # every program, with the frees on
hearth/kiln/tests/scripts/run_regression.sh
hearth/front/tests/scripts/check_types.sh            # ok_*_j.ash must check clean, err_*_j.ash must fail with the text
hearth/kiln/tests/scripts/region_check.sh            # region loops counted, run with poison, without regions, peak memory
hearth/ashvm/tests/scripts/memory_check.sh           # the memory figures
hearth/ashvm/tests/scripts/ownership_fuzz.sh         # random programs: same output with safety on and off
KILN=1 KILN_REGION_POISON=1 hearth/ashvm/tests/scripts/ownership_fuzz.sh
hearth/kiln/tests/scripts/windows_emulated_check.sh  # the PE output under the Unicorn emulator
hearth/front/tests/scripts/explain_check.sh          # every error code is documented above, and `ashvm explain` prints it
```

The test programs live in `hearth/ashvm/tests/cases/{ownership,safety,memoff}`, `hearth/kiln/tests/cases/ownership`
and `hearth/front/tests/cases/{types,run}`; the history of every rule is in `CHANGELOG.md`.
