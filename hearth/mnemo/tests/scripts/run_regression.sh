#!/usr/bin/env bash
# mnemo/tests/scripts/run_regression.sh
# Builds mnemo.a and runs the C programs in tests/cases/*/ against its public C ABI.
set -uo pipefail
cd "$(dirname "$0")/../.." # -> mnemo/
make -s || { echo "build failed"; exit 1; }
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
fail=0
for f in tests/cases/*/*.c; do
    gcc -Wall -Wextra -std=c11 -I.. "$f" ../../bin/lib/mnemo.a -lpthread -ldl -lm -o "$TMP/t" || { echo "DIFF: $f (compile)"; fail=1; continue; }
    if "$TMP/t" > "$TMP/out" 2>&1; then echo "OK:   $f"; else echo "DIFF: $f"; cat "$TMP/out"; fail=1; fi
done
exit $fail
