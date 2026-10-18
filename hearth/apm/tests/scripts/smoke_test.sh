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

"$BIN" add github:octocat/Hello-World > /dev/null || fail "add"
grep -q "Hello-World: github:octocat/Hello-World" ash.pkg || fail "dependency not recorded"
[ -d @ash-modules/Hello-World ] || fail "dependency not installed"

rm -rf @ash-modules
"$BIN" install > /dev/null || fail "install"
[ -d @ash-modules/Hello-World ] || fail "install did not restore the dependency"

"$BIN" install > /dev/null || fail "re-install (idempotency)"

"$BIN" list | grep -q "Hello-World" || fail "list didn't show the dependency"

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
Hello-World: github:octocat/Hello-World

[libraries]
m: system
vulkan: pkg-config:vulkan

[scripts]
build: echo building > built.txt
test: echo testing
PKG
"$BIN" add github:octocat/Spoon-Knife --dev > /dev/null || fail "add --dev"
for want in "^description: A demo package" "^author: Ada <ada@example.com>" "^license: MIT" "^main: demo.Main" \
            "^keywords: ash, demo" "^m: system" "^vulkan: pkg-config:vulkan" "^build: echo building" \
            "^\[dev-dependencies\]" "^Spoon-Knife: github:octocat/Spoon-Knife"; do
    grep -q -- "$want" ash.pkg || fail "ash.pkg lost '$want' after add --dev"
done
[ -d @ash-modules/Spoon-Knife ] || fail "dev dependency not installed"
"$BIN" list | grep -q "vulkan: pkg-config:vulkan" || fail "list didn't show libraries"

"$BIN" build > /dev/null || fail "apm build"
[ "$(cat built.txt)" == "building" ] || fail "build script did not run"
out=$("$BIN" run test) && grep -q "^testing$" <<<"$out" || fail "apm run test"
if "$BIN" run nonexistent > /dev/null 2>&1; then fail "unknown script should exit non-zero"; fi

if "$BIN" bogus-command > /dev/null 2>&1; then fail "unknown command should exit non-zero"; fi

echo "apm smoke test: OK"
