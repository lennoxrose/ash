#!/usr/bin/env bash
# ashvm/tests/scripts/run_regression.sh
# Runs every program through `ashvm run` and compares stdout+stderr byte for byte with the
# sibling .out file. Programs: ashvm/tests/cases/*/*.ash, the shared front-end cases
# (front/tests/cases/run/*_j.ash; *_wrap_* need --wrap-int) and ../../examples/*.ash.
set -uo pipefail
cd "$(dirname "$0")/../.." # -> ashvm/

make -s || { echo "build failed"; exit 1; }
VM=../../bin/ashvm
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT

fail=0
for f in tests/cases/*/*.ash ../front/tests/cases/run/*_j.ash ../../examples/*.ash; do
    flags=()
    case "$f" in *_wrap_*) flags=(--wrap-int);; esac
    rm -rf /tmp/ash_stdlib_test_dir /tmp/ash_example_io
    "$VM" run "${flags[@]}" "$f" > "$TMP/out" 2>&1
    if cmp -s "$TMP/out" "${f%.ash}.out"; then echo "OK:   $f"; else echo "DIFF: $f"; diff "$TMP/out" "${f%.ash}.out" | head -10; fail=1; fi
done
exit $fail
