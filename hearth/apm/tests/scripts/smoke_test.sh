#!/usr/bin/env bash
# apm/tests/scripts/smoke_test.sh -- exercises the CLI end to end
# (init/add/install/list/run) against a throwaway scratch directory. Not a
# .ash regression suite like ashvm/kiln have (apm doesn't run Ash
# programs, it manages their dependencies) -- this is the equivalent
# check for this project: does the real CLI, talking to the real
# filesystem and the real `git`, do what it says.
set -euo pipefail
cd "$(dirname "$0")/../.." # -> apm/

make -s

BIN="$(cd ../../bin && pwd)/apm" # absolute -- the script cd's into a scratch dir below
SCRATCH=$(mktemp -d)
trap 'rm -rf "$SCRATCH"' EXIT

fail() { echo "FAIL: $1"; exit 1; }

cd "$SCRATCH"

"$BIN" init > /dev/null || fail "init"
[ -f ash.pkg ] || fail "ash.pkg not created"
grep -q "^name:" ash.pkg || fail "ash.pkg missing name"

"$BIN" add github:lennoxrose/ash-stdlib > /dev/null || fail "add"
grep -q "ash-stdlib: github:lennoxrose/ash-stdlib" ash.pkg || fail "dependency not recorded"
[ -d @ash-modules/ash-stdlib ] || fail "dependency not installed"

rm -rf @ash-modules
"$BIN" install > /dev/null || fail "install"
[ -d @ash-modules/ash-stdlib ] || fail "install did not restore the dependency"

"$BIN" install > /dev/null || fail "re-install (idempotency)"

"$BIN" list | grep -q "ash-stdlib" || fail "list didn't show the dependency"

# full manifest: every field and section survives a load/save round trip (add rewrites ash.pkg)
cat > ash.pkg <<'PKG'
name: demo
version: 1.2.0
description: A demo package
author: Ada <ada@example.com>
license: MIT
homepage: https://example.com/demo
repository: github:ada/demo
bugs: https://example.com/demo/issues
main: demo.Main
keywords: ash, demo

[dependencies]
ash-stdlib: github:lennoxrose/ash-stdlib

[libraries]
m: system
vulkan: pkg-config:vulkan

[scripts]
build: echo building > built.txt
test: echo testing
PKG
"$BIN" add github:lennoxrose/ash-stdlib --as stdlib-dev --dev > /dev/null || fail "add --dev"
for want in "^description: A demo package" "^author: Ada <ada@example.com>" "^license: MIT" "^main: demo.Main" \
            "^keywords: ash, demo" "^m: system" "^vulkan: pkg-config:vulkan" "^build: echo building" \
            "^\[dev-dependencies\]" "^stdlib-dev: github:lennoxrose/ash-stdlib"; do
    grep -q -- "$want" ash.pkg || fail "ash.pkg lost '$want' after add --dev"
done
[ -d @ash-modules/stdlib-dev ] || fail "dev dependency not installed"
"$BIN" list | grep -q "vulkan: pkg-config:vulkan" || fail "list didn't show libraries"

"$BIN" build > /dev/null || fail "apm build"
[ "$(cat built.txt)" == "building" ] || fail "build script did not run"
out=$("$BIN" run test) && grep -q "^testing$" <<<"$out" || fail "apm run test"
if "$BIN" run nonexistent > /dev/null 2>&1; then fail "unknown script should exit non-zero"; fi

# licenses: write LICENSE and keep ash.pkg in sync; running again replaces both
"$BIN" license MIT --year 2025 > /dev/null || fail "apm license MIT"
grep -q "^Copyright (c) 2025 Ada <ada@example.com>$" LICENSE && fail "email must not end up in the copyright line"
grep -q "^Copyright (c) 2025 Ada$" LICENSE || fail "LICENSE copyright line (author from ash.pkg)"
grep -q "^license: MIT$" ash.pkg || fail "ash.pkg license not set to MIT"
"$BIN" license gpl-3.0-only > /dev/null || fail "apm license GPL-3.0 (alias, replace)"
grep -q "GNU GENERAL PUBLIC LICENSE" LICENSE || fail "LICENSE not replaced"
grep -q "^license: GPL-3.0$" ash.pkg || fail "ash.pkg license not updated"
grep -q "^build: echo building" ash.pkg || fail "ash.pkg lost other fields on license change"
"$BIN" license | grep -q "OFL-1.1" || fail "license list"
if "$BIN" license no-such-license > /dev/null 2>&1; then fail "unknown license should exit non-zero"; fi

# new + check: scaffold follows the conventions once a license is chosen
"$BIN" new demo-mod > /dev/null || fail "apm new"
[ -f demo-mod/src/main/ash/demo_mod/DemoMod.ash ] || fail "apm new source file"
(cd demo-mod && if "$BIN" check > /dev/null 2>&1; then fail "check should flag the missing license"; fi
 "$BIN" license MIT --author t > /dev/null && "$BIN" check > /dev/null) || fail "apm check on scaffold"
if "$BIN" new demo-mod > /dev/null 2>&1; then fail "apm new over existing dir should fail"; fi

# ash.pkg is edited, not regenerated: comments, blank lines, unknown keys and sections and the author's order survive
mkdir keep && (cd keep
printf '# my package\nname: keep\nversion: 1.0.0\nmemory_safety: off\n\n# scripts\n[scripts]\n# build it\nbuild: echo hi\n\n[weird]\nk: v\n' > ash.pkg
"$BIN" license MIT --author me > /dev/null 2>&1 || fail "license on a hand-written ash.pkg"
for want in "^# my package$" "^memory_safety: off$" "^# scripts$" "^# build it$" "^\[weird\]$" "^k: v$" "^license: MIT$"; do
    grep -q -- "$want" ash.pkg || fail "ash.pkg lost '$want' when apm rewrote it"
done
[ "$(grep -n '^license: MIT$' ash.pkg | cut -d: -f1)" -lt "$(grep -n '^\[scripts\]$' ash.pkg | cut -d: -f1)" ] || fail "new header key must land in the header")
# a script longer than 255 characters (the old value limit) is kept whole and runs whole
long=$(printf 'x%.0s' $(seq 600))
mkdir longline && (cd longline
printf 'name: l\nversion: 1\n\n[scripts]\nbig: echo %s > out.txt\n' "$long" > ash.pkg
"$BIN" run big > /dev/null || fail "apm run of a long script"
[ "$(wc -c < out.txt)" -eq 601 ] || fail "long script was cut"
"$BIN" license MIT --author me > /dev/null 2>&1
grep -q "^big: echo $long > out.txt$" ash.pkg || fail "long script cut by a rewrite")

# a dependency name is a directory and ends up on a git command line: it must never reach a shell
mkdir inject && (cd inject
printf 'name: i\nversion: 1\n\n[dependencies]\nx; touch pwned: github:lennoxrose/ash-stdlib\n../escape: github:lennoxrose/ash-stdlib\n' > ash.pkg
if "$BIN" install > /dev/null 2>&1; then fail "install must reject unusable dependency names"; fi
[ ! -e pwned ] || fail "a dependency name was run by a shell"
[ ! -e ../escape ] || fail "a dependency name escaped @ash-modules"
if "$BIN" add github:lennoxrose/ash-stdlib --as '../x' > /dev/null 2>&1; then fail "add --as ../x must be rejected"; fi
if "$BIN" add github:lennoxrose/ash-stdlib --bogus > /dev/null 2>&1; then fail "add with an unknown option must fail"; fi
if "$BIN" add 'github:lennoxrose/ash-stdlib;touch' > /dev/null 2>&1; then fail "add with a bad spec must fail"; fi
[ ! -e touch ] || fail "a source spec was run by a shell")

# a dependency that cannot be fetched is not recorded
mkdir norepo && (cd norepo
printf 'name: n\nversion: 1\n' > ash.pkg
cp ash.pkg before.pkg
if "$BIN" add github:lennoxrose/ash-no-such-repo-0xdeadbeef > /dev/null 2>&1; then fail "add of a missing repository must fail"; fi
cmp -s ash.pkg before.pkg || fail "failed add changed ash.pkg")

# apm check: unknown keys, memory-safety settings, dependency specs, main
"$BIN" new chk > /dev/null && (cd chk
"$BIN" license MIT --author t > /dev/null
"$BIN" check > /dev/null 2>&1 || fail "check on a clean scaffold"
grep -q "^memory-safety: on$" ash.pkg || fail "scaffold should set memory-safety: on"
sed -i 's/^memory-safety: on$/memory_safety: on/' ash.pkg
out=$("$BIN" check 2>&1) || fail "an unknown key is a warning, not a failure"
grep -q "unknown key 'memory_safety'.*did you mean 'memory-safety'" <<<"$out" || fail "check: unknown key not reported ($out)"
sed -i 's/^memory_safety: on$/memory-safety: false/' ash.pkg
if out=$("$BIN" check 2>&1); then fail "memory-safety: false must fail the check"; fi
grep -q "memory-safety must be 'on' or 'off'" <<<"$out" || fail "check: bad memory-safety value not reported ($out)"
sed -i 's/^memory-safety: false$/memory-safety: off/' ash.pkg
out=$("$BIN" check 2>&1) || fail "memory-safety: off is a warning"
grep -q "memory-safety is off" <<<"$out" || fail "check: memory-safety off not mentioned ($out)"
sed -i '/^memory-safety: off$/d' ash.pkg
printf '\n[scripts]\nmemory-safety: off\n' >> ash.pkg
out=$("$BIN" check 2>&1) || fail "memory-safety under a section is a warning"
grep -q "only reads it in the header" <<<"$out" || fail "check: misplaced memory-safety not reported ($out)"
sed -i '/^memory-safety: off$/d' ash.pkg
printf '\n[dependencies]\nlib: path:../lib\nbad;name: github:u/r\n' >> ash.pkg
if out=$("$BIN" check 2>&1); then fail "unusable dependencies must fail the check"; fi
grep -q "'path:../lib' is not a source apm can fetch" <<<"$out" || fail "check: bad source not reported ($out)"
grep -q "'bad;name' is not a usable name" <<<"$out" || fail "check: bad name not reported ($out)"
sed -i '/^\[dependencies\]$/,$d' ash.pkg
sed -i 's/^main: .*/main: nope.Missing/' ash.pkg
if out=$("$BIN" check 2>&1); then fail "a main that does not exist must fail the check"; fi
grep -q "main 'nope.Missing' is not a file" <<<"$out" || fail "check: missing main not reported ($out)")

if "$BIN" bogus-command > /dev/null 2>&1; then fail "unknown command should exit non-zero"; fi

echo "apm smoke test: OK"
