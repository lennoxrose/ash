#!/usr/bin/env bash
# kiln/tests/imports_circular_check.sh -- not part of run_regression.sh's
# stdout-diff loop, since this test asserts a COMPILE FAILURE with
# specific message content, not a successful program's stdout.
set -uo pipefail
cd "$(dirname "$0")/.."

OUT=$(../bin/kiln tests/fixtures/imports_circular_a.ash -o /tmp/imports_circular_a 2>&1)
STATUS=$?

if [[ $STATUS -eq 0 ]]; then
    echo "FAIL: expected nonzero exit, got 0"
    exit 1
fi
if [[ "$OUT" != *"circular import"* ]]; then
    echo "FAIL: expected 'circular import' in output, got:"
    echo "$OUT"
    exit 1
fi
if [[ "$OUT" != *"imports_circular_a.ash"* || "$OUT" != *"imports_circular_b.ash"* ]]; then
    echo "FAIL: expected both file names in the cycle message, got:"
    echo "$OUT"
    exit 1
fi

echo "OK: imports_circular_check"
