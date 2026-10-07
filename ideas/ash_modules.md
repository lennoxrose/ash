# Ash Modules — a package manager for Ash

> **TL;DR:** An npm-like package manager for Ash. Packages get fetched into a project-local `@ash-modules/` folder (same idea as `node_modules`). A manifest file (`ash.pkg`) declares dependencies and versions. `@import <name>;` — which already works for local `ash.libs/` folders — gets extended to also search `@ash-modules/`, so existing code and syntax don't change.

---

## Table of Contents

1. [Goals & Non-Goals](#goals--non-goals)
2. [What Already Exists](#what-already-exists)
3. [The Manifest: `ash.pkg`](#the-manifest-ashpkg)
4. [`@ash-modules/` Layout](#ash-modules-layout)
5. [Resolution Order](#resolution-order)
6. [Versioning](#versioning)
7. [The Registry](#the-registry)
8. [CLI: `forgepack`](#cli-forgepack)
9. [Publishing a Package](#publishing-a-package)
10. [Development Phases](#development-phases)
11. [Design Principles](#design-principles)
12. [Risks & Open Questions](#risks--open-questions)

---

## Goals & Non-Goals

**Goals**

- `@import <name>;` keeps working exactly as it does today — a package is just something else `<name>` can resolve to.
- A manifest file per project declares what it depends on and at what version.
- Installed dependencies live in a project-local folder (`@ash-modules/`), never globally — two projects can depend on different versions of the same package with zero conflict.
- Works identically for `ashvm` and `kiln` — a package is plain `.ash` source, and both engines already share one `@import` syntax.
- Lockfile for reproducible installs.

**Non-Goals**

- A general-purpose build system (Ash programs are single-entry-point scripts; this is dependency resolution, not a `Makefile` replacement).
- Native/binary dependencies (a package is Ash source only — no compiled artifacts, no FFI, at least for v1).
- A hosted registry on day one (see [The Registry](#the-registry) — start file/git-based, add a real registry later).
- Semver solving across a huge transitive graph with conflicting ranges — start with a simple model (see [Versioning](#versioning)) and only grow it if real packages need it.

---

## What Already Exists

Don't design this from scratch — `@import` already has most of the shape a package system needs:

```ash
@import <./fixtures/helper.ash>;  // relative path, must end in .ash
@import <testlib>;                // bare name, no '.' or '/' allowed
```

A bare name already resolves (`compiler/C/import_paths.c`, mirrored in `kiln`) to:

```text
<project_root>/ash.libs/<name>/<name>.ash
```

That's a real, working, local "library folder" convention — it's just not fetchable. Ash Modules reuses the exact same bare-name syntax and the exact same "no `.` or `/`" rule, and adds a *second* place that name is allowed to resolve to: `@ash-modules/<name>/`. `ash.libs/` keeps working unchanged for hand-written local libraries that never need a version or a registry.

---

## The Manifest: `ash.pkg`

One file per project, at the project root (same level as `ash.libs/`), plain key-value text — not JSON, to match Ash's own "no external dependencies" ethos (no need to pull in a JSON parser just to read a manifest):

```text
name: my-project
version: 0.1.0

[dependencies]
http: 2.1.0
json: ^1.3.0
left-pad: 1.0.0
```

- `[dependencies]` lists direct dependencies only — transitive ones are resolved and recorded in the lockfile, not hand-written here.
- A package itself has the same file at its own root (so a dependency can declare its *own* dependencies) — a package and a project are the same kind of thing, the only difference is whether anything depends on it.
- `name`/`version` double as what gets published to the registry (see below).

---

## `@ash-modules/` Layout

```text
my-project/
├── ash.pkg
├── ash.lock
├── main.ash
├── ash.libs/              # unchanged — hand-written local libs, never fetched
│   └── mathutils/
│       └── mathutils.ash
└── @ash-modules/          # generated, gitignored, never hand-edited
    ├── http/
    │   ├── ash.pkg
    │   └── http.ash
    ├── json/
    │   ├── ash.pkg
    │   └── json.ash
    └── left-pad/
        ├── ash.pkg
        └── left-pad.ash
```

- Flat, not nested — same lesson npm learned the hard way (npm 2's deeply nested `node_modules` was replaced by npm 3's flat layout). A dependency's *own* dependencies also land flat in the top-level `@ash-modules/`, deduped by version where possible.
- A version conflict (two dependencies need incompatible versions of the same package) gets a single nested fallback: `@ash-modules/<name>/@ash-modules/<name>@<version>/` — resolution always checks the nearest `@ash-modules/` first, same lookup order Node uses for `node_modules`.
- `@ash-modules/` is machine-generated and gitignored, like `O/`/`D/` already are for build output — `forgepack install` regenerates it from `ash.lock`.

---

## Resolution Order

`@import <name>;` tries, in order, from the importing file's project root:

1. `ash.libs/<name>/<name>.ash` — unchanged, hand-written local libraries win first (a project can always shadow a package name with its own local copy).
2. `@ash-modules/<name>/<name>.ash` — the installed package's entry file, named the same as the package itself (matches the existing `ash.libs` convention exactly, so the resolver code barely changes).
3. Walk up: if the current directory has no `@ash-modules/<name>/`, check the parent's, same as Node's `node_modules` walk — lets a package's own `@import`s resolve against the *project's* installed copies instead of needing its own nested set for every shared dependency.

No new import syntax. The only change is `import_resolve_path()` gaining a second candidate path to try before giving up.

---

## Versioning

Semver (`MAJOR.MINOR.PATCH`), kept deliberately simple for v1:

- An exact version (`2.1.0`) always means exactly that version.
- A caret range (`^1.3.0`) means "`>=1.3.0`, `<2.0.0`" — the usual meaning, allows patch/minor updates, blocks breaking ones.
- No `~`, no `>=<`, no OR-ranges in v1 — if a real package needs more than caret ranges, add it then, not speculatively now.
- `ash.lock` pins every resolved package (direct and transitive) to one exact version and a content hash, so `forgepack install` is always reproducible — the manifest describes *intent*, the lockfile describes *what actually got installed*.

---

## The Registry

Start without a hosted server at all — two sources, both file-based:

1. **Git.** `forgepack add github:someone/some-package` clones a tag/commit straight into `@ash-modules/`. Zero infrastructure, works day one, same escape hatch cargo/npm/go modules all still support.
2. **A flat static index**, once there's more than a handful of packages: a single JSON (or plain-text) file mapping `name -> [versions] -> tarball URL`, served from anywhere static (GitHub Pages, a S3 bucket) — no backend service, no database, no auth to build first. `forgepack publish` is just "upload a tarball + update the index."

A real registry with search, download counts, a web UI — all later, once there's something worth searching. Don't build npm's infrastructure for zero packages.

---

## CLI: `forgepack`

```text
forgepack init                  # creates ash.pkg in the current directory
forgepack install                # reads ash.pkg + ash.lock, populates @ash-modules/
forgepack add <name>[@version]   # adds to ash.pkg, resolves, installs, updates ash.lock
forgepack add github:user/repo   # git-sourced dependency
forgepack remove <name>          # removes from ash.pkg, re-resolves, prunes @ash-modules/
forgepack update [<name>]        # re-resolves within manifest ranges, rewrites ash.lock
forgepack publish                # packages the current directory, pushes to the index
forgepack list                   # prints the resolved dependency tree
```

Sample `list` output:

```text
my-project@0.1.0
├── http@2.1.0
│   └── json@1.4.2
├── json@1.3.0          (direct — different version than http's own json dependency)
└── left-pad@1.0.0
```

---

## Publishing a Package

A package is just a project with no `main` entry point expectation — `ash.pkg`'s `name`/`version`, a `<name>.ash` entry file at the root, optionally its own `ash.libs/`/dependencies. `forgepack publish`:

1. Validates `ash.pkg` (name/version present, version not already published).
2. Tars up the directory (minus `@ash-modules/`, minus anything in a `.ashignore`).
3. Uploads the tarball and updates the index (or, for the git-only path, publishing *is* just tagging a commit — nothing to run).

---

## Development Phases

| Phase | Goal | Details |
|-------|------|---------|
| **1. Manifest + local install** | Prove the shape works | `ash.pkg` parsing, `@ash-modules/` resolution added to `import_resolve_path()`, `forgepack install` reading only git/local-path sources (no index yet). |
| **2. Lockfile** | Reproducible installs | `ash.lock` generation, content hashing, `forgepack install` becoming deterministic from the lockfile alone. |
| **3. Static index** | Discoverability without a server | Flat JSON index, `forgepack publish`/`add <name>` against it, version resolution against caret ranges. |
| **4. Version conflict handling** | Real-world dependency graphs | Nested fallback resolution, `forgepack list` tree view, dedup pass. |
| **5. Registry service** *(maybe, later)* | Search, stats, web UI | Only once there's enough real packages to justify it. |

---

## Design Principles

1. **`@import` doesn't change.** A package is just another thing a bare name can resolve to — no new syntax, no special "package import" keyword.
2. **Project-local, never global.** No version conflicts between unrelated projects, no `sudo` to install a package, matches how `@ash-modules/` already sits next to `ash.libs/`.
3. **The manifest describes intent; the lockfile describes reality.** Never let `install` silently drift from what got reviewed/committed.
4. **No hosted infrastructure until it's earned.** Git and a static file are a real registry for a first ecosystem; a server is a later problem.
5. **Packages are plain Ash.** No native code, no FFI, no build step inside a package — anything that can `@import` a package can also just read it.

---

## Risks & Open Questions

- **Name squatting / collisions** once there's a shared static index — needs *some* claiming rule before the index is writable by anyone.
- **Git dependency pinning:** a tag can move, a branch definitely does. `ash.lock` should probably pin a commit hash for git sources, not just a tag name.
- **Circular package dependencies:** `ashvm`'s `@import` already handles circular *file* imports (see `imports_circular_*` tests) — need to confirm that still holds once "file" becomes "a file inside someone else's installed package."
- **Namespace clashes with `ash.libs/`:** a project with a local `ash.libs/http/` and a dependency also named `http` — local wins per the resolution order above, but should probably at least warn.
- **Removing packages:** `forgepack remove` pruning `@ash-modules/` safely needs to know what's still a transitive dependency of something else first.
- **No native deps today — forever?** If a package ever genuinely needs to call into C (e.g. for something `kiln` can't do in pure Ash), there's no story for that yet. Worth deciding explicitly rather than backing into it.
