#!/usr/bin/env bash
# kiln/tests/scripts/imports_collision_check.sh -- asserts BOTH resolved paths
# appear in the error message, not just that a compile error fires (see
# the design spec's "Testing plan" section -- with no `as` clause to
# disambiguate, this message is the only diagnostic a user gets).
set -uo pipefail
cd "$(dirname "$0")/../.."

OUT=$(../bin/kiln tests/cases/imports/fixtures/imports_collision_main.ash -o /tmp/imports_collision_main 2>&1)
STATUS=$?

if [[ $STATUS -eq 0 ]]; then
    echo "FAIL: expected nonzero exit, got 0"
    exit 1
fi
if [[ "$OUT" != *"namespace"* ]]; then
    echo "FAIL: expected 'namespace' in output, got:"
    echo "$OUT"
    exit 1
fi
if [[ "$OUT" != *"collision_dir_one/shared.ash"* || "$OUT" != *"collision_dir_two/shared.ash"* ]]; then
    echo "FAIL: expected BOTH colliding resolved paths in the message, got:"
    echo "$OUT"
    exit 1
fi

echo "OK: imports_collision_check"
