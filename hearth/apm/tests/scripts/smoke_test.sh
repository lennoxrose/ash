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

if "$BIN" bogus-command > /dev/null 2>&1; then fail "unknown command should exit non-zero"; fi

echo "apm smoke test: OK"
