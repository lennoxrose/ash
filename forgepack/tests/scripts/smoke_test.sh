#!/usr/bin/env bash
# forgepack/tests/scripts/smoke_test.sh -- exercises the CLI end to end
# (init/add/install/list) against a throwaway scratch directory. Not a
# .ash regression suite like ashvm/kiln have (forgepack doesn't run Ash
# programs, it manages their dependencies) -- this is the equivalent
# check for this project: does the real CLI, talking to the real
# filesystem and the real `git`, do what it says.
set -euo pipefail
cd "$(dirname "$0")/../.." # -> forgepack/

make -s

BIN="$(cd ../bin && pwd)/forgepack" # absolute -- the script cd's into a scratch dir below
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

if "$BIN" bogus-command > /dev/null 2>&1; then fail "unknown command should exit non-zero"; fi

echo "forgepack smoke test: OK"
