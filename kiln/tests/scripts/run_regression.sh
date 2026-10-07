#!/usr/bin/env bash
# kiln/tests/scripts/run_regression.sh baseline|check
set -uo pipefail
cd "$(dirname "$0")/../.." # -> kiln/

MODE="${1:-check}"
if [[ "$MODE" != "baseline" && "$MODE" != "check" ]]; then
    echo "usage: $0 baseline|check" >&2
    exit 2
fi

OUTDIR="/tmp/kiln_regression/$MODE"
BINDIR="/tmp/kiln_regression/bin"
rm -rf "$OUTDIR"
mkdir -p "$OUTDIR"
mkdir -p "$BINDIR"

# Clean up test files that don't clean up after themselves
rm -f /tmp/kiln_file_test.txt

make -s || { echo "build failed"; exit 1; }

fail=0
for f in tests/cases/*/*.ash; do
    name=$(basename "$f" .ash)
    bin="$BINDIR/${name}.bin"
    out="$OUTDIR/${name}.out"
    if ! ../bin/kiln "$f" -o "$bin" > "$out" 2>&1; then
        echo "COMPILE FAIL" >> "$out"
        continue
    fi
    if [[ "$name" == "input_builtin" ]]; then
        printf 'World\n5\n' | "$bin" >> "$out" 2>&1
    else
        "$bin" >> "$out" 2>&1
    fi
done

if [[ "$MODE" == "baseline" ]]; then
    echo "baseline captured in $OUTDIR"
    exit 0
fi

for f in "$OUTDIR"/*.out; do
    name=$(basename "$f" .out)
    base="/tmp/kiln_regression/baseline/${name}.out"
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
exit $fail
