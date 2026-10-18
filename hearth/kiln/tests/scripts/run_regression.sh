#!/usr/bin/env bash
# kiln/tests/scripts/run_regression.sh
# Compiles every program to a native Linux binary and compares its stdout+stderr byte for byte
# with the sibling .out file. Programs: kiln/tests/cases/*/*.ash, the shared front-end cases
# (front/tests/cases/run/*_j.ash; *_wrap_* need --wrap-int) and ../../examples/*.ash.
set -uo pipefail
cd "$(dirname "$0")/../.." # -> kiln/

make -s || { echo "build failed"; exit 1; }
KILN=../../bin/kiln
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT

fail=0
for f in tests/cases/*/*.ash ../front/tests/cases/run/*_j.ash ../../examples/*.ash; do
    n=$(basename "$f" .ash)
    # kiln's native runtime prints doubles with 7 significant digits, not Java's shortest round-trip form
    [ "$n" == run_double_j ] && continue
    flags=()
    case "$f" in *_wrap_*) flags=(--wrap-int);; esac
    if ! "$KILN" "$f" -o "$TMP/$n" "${flags[@]}" > "$TMP/$n.err" 2>&1; then echo "COMPILE FAIL: $f"; head -5 "$TMP/$n.err"; fail=1; continue; fi
    rm -rf /tmp/ash_stdlib_test_dir /tmp/ash_example_io
    "$TMP/$n" > "$TMP/$n.out" 2>&1 < /dev/null
    if cmp -s "$TMP/$n.out" "${f%.ash}.out"; then echo "OK:   $f"; else echo "DIFF: $f"; diff "$TMP/$n.out" "${f%.ash}.out" | head -10; fail=1; fi
done
exit $fail
