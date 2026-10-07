#!/usr/bin/env bash
# ashvm/tests/scripts/run_regression.sh baseline|check
# Compiles+runs every ashvm/tests/cases/*/*.ash and diffs stdout against a
# captured baseline -- mirrors kiln/tests/scripts/run_regression.sh. ashvm has no
# --target/--link flags and no input()-driven test today, so this is
# simpler than kiln's version.
set -uo pipefail
cd "$(dirname "$0")/../.." # -> ashvm/

MODE="${1:-check}"
if [[ "$MODE" != "baseline" && "$MODE" != "check" ]]; then
    echo "usage: $0 baseline|check" >&2
    exit 2
fi

OUTDIR="/tmp/ashvm_regression/$MODE"
rm -rf "$OUTDIR"
mkdir -p "$OUTDIR"

rm -f /tmp/ash_test.txt

make -s || { echo "build failed"; exit 1; }

for f in tests/cases/*/*.ash; do
    name=$(basename "$f" .ash)
    out="$OUTDIR/${name}.out"
    if ! ../bin/ashvm "$f" > "$out" 2>&1; then
        echo "RUN FAIL" >> "$out"
    fi
done

if [[ "$MODE" == "baseline" ]]; then
    echo "baseline captured in $OUTDIR"
    exit 0
fi

fail=0
for f in "$OUTDIR"/*.out; do
    name=$(basename "$f" .out)
    base="/tmp/ashvm_regression/baseline/${name}.out"
    if [[ ! -f "$base" ]]; then
        echo "NEW: $name (no baseline to compare)"
        continue
    fi
    if diff -q "$base" "$f" > /dev/null; then
        echo "OK:  $name"
    else
        echo "DIFF: $name"
        diff "$base" "$f"
        fail=1
    fi
done
if ! ./tests/scripts/imports_circular_check.sh; then
    fail=1
fi
if ! ./tests/scripts/imports_collision_check.sh; then
    fail=1
fi

exit $fail
