# Releasing Ash

One command cuts a release:

```sh
make release VERSION=beta-v0.2.0                  # builds, tests, packages, asks, then publishes
make release VERSION=beta-v0.2.0 ARGS=--dry-run   # everything except tag / push / publish
tools/release.sh beta-v0.2.0 --yes                # same thing without `make`, no confirmation prompt
```

Tags are `beta-vX.Y.Z` or `vX.Y.Z`. The release is published as the **latest** release and never as a
pre-release (GitHub's `/releases/latest/download/...` ignores pre-releases); "beta" lives in the tag, the
title (`Ash <tag>`) and the notes.

## Prerequisites

- On `main`, clean tree, equal to `origin/main` (push first). A dry run only reports this.
- A `## YYYY-MM-DD (<tag> ...)` section at the top of `CHANGELOG.md`. It becomes the release notes; the
  tool stops if it is missing.
- `git`, `gh` (logged in: `gh auth login`), `make`, `gcc`, `cargo` (mnemo; `tools/install.sh`), GNU `tar`,
  `sha256sum`, `curl`, `bwrap` (bubblewrap, the smoke-test sandbox).
- A checkout of `ash-stdlib` next to this repo (the test scripts read `../ash-stdlib/src/main/ash`) and
  network access (the `apm` smoke test clones from GitHub).

## What the tool does

1. Checks the tag format and that the tag exists neither locally nor on origin; checks git state and `gh`.
2. `make clean && make` with `ASH_VERSION=<tag>` and `ASH_STATIC_RT=1` exported to every nested make.
3. Runs the mnemo, type-check, ashvm, kiln and apm suites. Any failure aborts.
4. Builds `dist/ash-<os>-<arch>.tar.gz` (`bin/ashvm`, `bin/kiln`, `bin/apm`, `VERSION`; stripped copies,
   nothing wrapped in a folder) and `dist/SHA256SUMS`.
5. Smoke test: extracts the tarball and runs it in a `bwrap` sandbox that contains only the shared
   libraries `ldd` lists for the three binaries (no compiler, no cargo, no PATH, no network): `--version`
   of each tool, `ashvm run` and `kiln` + run on `examples/HelloWorld.ash`, `apm new`.
6. `sha256sum -c`, then a summary. Without `--dry-run` it asks before tagging (unless `--yes`).
7. Annotated tag, `git push origin <tag>`, `gh release create <tag> dist/* --latest`.
8. Downloads `SHA256SUMS` and the tarball from `.../releases/latest/download/` and verifies them.

Install location is `~/.ash`: extracting a tarball there gives `~/.ash/bin/ashvm` etc. Nothing else is
needed at run time: `kiln` writes static executables and finds no files of its own.

## Adding a platform

The tool builds and tests the machine it runs on, so a platform needs a machine of that kind.

1. On that machine run the tool; add the platform name (`<os>-<arch>`, `uname -s` lowercased and
   `uname -m` normalised to `x86_64` / `aarch64`) to `SUPPORTED` in `tools/release.sh` once it passes
   the whole flow, including the smoke test.
2. Its tarball must be named `ash-<os>-<arch>.tar.gz` with the same layout. Attach it to the existing
   release with `gh release upload <tag> ash-<os>-<arch>.tar.gz`, then regenerate `SHA256SUMS` for all
   tarballs and upload it with `--clobber`. Do not ship a platform that was not built and run.

## Fixing a bad release

Never move or reuse a tag. Delete the release and the tag, then release the next version:

```sh
gh release delete <tag> --cleanup-tag --yes     # removes the release and the remote tag
git tag -d <tag>                                # the local tag
make release VERSION=<next tag>                 # with a CHANGELOG section for the next tag
```
