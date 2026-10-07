# Kiln

a standalone `.ash` compiler - not an interpreter -
working on Windows and Linux (macOS best-effort), invoked like
`kiln example.ash -o example`, with its own linker that eventually shares
a common runtime library across compiled binaries instead of duplicating
it in every executable.

Zero external tools anywhere in the pipeline: no gcc/clang/ld/as. Kiln
hand-encodes x86-64 machine code and writes its own ELF executables
directly, the same "every line hand-written" rule that built `ashc` and
`ashvm`.

## Status: milestone 1 done (Linux only)

`kiln file.ash -o out` compiles `print <expr>;` programs (integer literals,
`+ - * /`, unary `-`, parens, multiple statements) straight to a real,
standalone, freestanding ELF64 executable - no libc, raw syscalls only.

```bash
cd kiln && make          # builds ../bin/kiln
../bin/kiln tests/cases/basics/hello.ash -o /tmp/hello
/tmp/hello                # prints 7  (1 + 2 * 3)
```

## What's deliberately not built yet

- Everything beyond numeric `print` statements - variables, functions,
  strings, arrays, control flow, closures
- Multi-object linking (the whole program is one self-contained object
  right now; there's no symbol-resolution step yet)
- The actual shared-runtime-library dedup feature goal 2 is ultimately
  about - that needs real dynamic linking (PIC codegen, dynamic symbol
  table, PLT/GOT, ELF dynamic sections), a substantial milestone of its
  own, deliberately not tackled alongside "does codegen produce a runnable
  binary at all"
- Windows PE output, macOS Mach-O output

## Layout

```
kiln/
  Makefile                builds ../bin/kiln
  src/                    each subsystem: C/ (sources), H/ (headers), D/ (deps), O/ (objects), mirrored
    app/                   main.c argv parsing, drives the pipeline; target.c/.h
    lexer/                 tokenizer (own copy, same shape as ashvm's)
    parser/C/ H/ D/ O/
      core/                  parser entry, shared parser state
      statements/            control flow, for loops, try/catch, loop stack
      declarations/          variables, functions, constants
      imports/               import resolution
    codegen/C/ H/ D/ O/    (same feature folders in each tree)
      emit/                  raw x86-64 encoding, byte buffers, memory layout, value tags
      expressions/           expression -> arithmetic instructions, calls, booleans
      functions/             closures and lambdas
      runtime/               heap, runtime errors, int printing
      collections/           arrays, maps, keys/values
      strings/               string values, allocation, string builtins
      builtins/              builtin dispatch, math, map/filter/reduce, str/num
      io/                    file, stdin and argv builtins
      platform/              console/file and Windows-call wrappers
    elf/                   ELF executable and shared-object writers
    pe/                    PE executable and DLL writers
    diagnostics/           error reporting
  tests/                  cases/<feature>/*.ash programs (+ scripts/run_regression.sh)
```