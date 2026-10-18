<p align="center">
  <img src="https://static.wikia.nocookie.net/godofwar/images/f/ff/Mnemosyne-gabriel-dante-rosetti-1881.jpg/revision/latest?cb=20241211010522" alt="Mnemosyne by Dante Gabriel Rossetti, 1881" width="400">
</p>

<h1 align="center">Mnemosyne</h1>

<p align="center">
  <em>Memory safety for the Ash language.</em>
</p>

---

## Who is Mnemosyne?

In Greek mythology, Mnemosyne is the Titaness of memory. She is the mother of the nine Muses, because the Greeks believed nothing could be created without first remembering. She keeps track of everything that was and everything that is.

That is exactly the job of this library: keep track of every piece of memory, and make sure nobody touches what they shouldn't.

## Layout

```
bin/
├── kiln
├── ashvm
└── lib/
    ├── front.a
    └── mnemo.a
```

## What is in it

`mnemo.a` is a Rust static library with a C ABI (`H/flow/flow.h`), linked by both `ashvm` and `kiln`. It is the move-flow engine of the ownership pass in `hearth/front/src/jown`: the pass walks the typed AST and reports moves, uses, branches and loops; Mnemosyne keeps the data-flow state (which variables are moved on the current path, joined at every branch and loop back edge).

```
hearth/mnemo/
├── Rust/src/flow.rs       the engine (cargo crate, built to bin/lib/mnemo.a by the Makefile)
├── H/flow/flow.h          the C ABI contract: ownership, errors, thread safety
└── tests/                 C programs that call the public ABI: tests/scripts/run_regression.sh
```

The pass decides what is moved, borrowed and freed; see "Memory safety" in the top-level README.

## Why "Mnemosyne"?

Because memory deserves a goddess. And because almost nobody has heard of her, which makes the name more fun.

---

<p align="center">
  Part of the <a href="https://ash-project.org">Ash</a> project.
</p>