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

## Usage

```bash
make @kiln                                   # builds ../bin/kiln
bin/kiln examples/HelloWorld.ash -o /tmp/hello
/tmp/hello                                   # Hello, Ash!

kiln <file.ash> -o <output> [--target=linux|windows] [--link=static|shared] [--wrap-int]
kiln --emit-runtime -o <libkilnrt.so|libkilnrt.dll> [--target=linux|windows]
```

Kiln compiles Java-like Ash (packages, classes, generics, ...) to native ELF (Linux) or PE (Windows)
executables. It has no frontend of its own: `hearth/front` (lexer, parser, type checker, lowering) is
linked in and hands kiln the internal IR (`hearth/front/src/ir`), which `src/parser` turns into machine
code in one pass. Old pre-migration scripting syntax is rejected by the front end.

## Layout

```
kiln/
  Makefile                builds ../bin/kiln
  src/                    each subsystem: C/ (sources), H/ (headers), D/ (deps), O/ (objects), mirrored
    app/                   main.c argv parsing, drives the pipeline; target.c/.h
    parser/C/ H/ D/ O/     one-pass IR -> machine code (the IR lexer lives in front/src/ir)
      core/                  parser entry, shared parser state
      statements/            control flow, loops, try/catch lowering, loop stack
      declarations/          variables, functions, constants
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
  tests/                  cases/<feature>/*.ash programs (+ scripts/run_regression.sh, which also runs
                          hearth/front/tests/cases/run and examples/)
```