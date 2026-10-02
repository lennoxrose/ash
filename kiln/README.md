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
../bin/kiln tests/hello.ash -o /tmp/hello
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
  Makefile              builds ../bin/kiln
  src/
    lexer/               tokenizer (own copy, same shape as ashvm's)
    parser/               token state + statement grammar
    codegen/
      emit.c/.h            raw x86-64 instruction encoding primitives
      expr.c/.h             expression -> real arithmetic instructions
      print_int.c/.h        int-to-decimal-ASCII + write(2) + exit(2)
    elf/
      elf_writer.c/.h        minimal "tiny ELF" executable writer
    main.c                  argv parsing, drives the pipeline
  tests/                  *.ash programs exercised by hand for now
```