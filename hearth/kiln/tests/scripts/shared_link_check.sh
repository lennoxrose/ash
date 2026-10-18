#!/usr/bin/env bash
# kiln/tests/scripts/shared_link_check.sh
#
# --link=shared coverage (run_regression.sh only compiles with kiln's default --link=static).
#
# 1. Rebuilds kiln and libkilnrt.so, installing it at kiln's hardcoded rpath (KILN_RPATH,
#    elf/H/elf_dynamic.h) -- there is no env-var override, so this writes to /usr/local/lib/kiln
#    for real, same as doing it by hand.
# 2. Compiles a sample of programs BOTH --link=static and --link=shared and diffs their output --
#    proving the two modes agree wherever both can run a program, which also means the combined
#    GOT/hash/dynsym table didn't disturb kiln's own runtime imports (heap_alloc, print_top, ...).
set -uo pipefail
cd "$(dirname "$0")/../.." # -> kiln/
# the optional standard library (ash-stdlib) is a sibling checkout unless ASH_PATH says otherwise
export ASH_PATH="${ASH_PATH:-$PWD/../../../ash-stdlib/src/main/ash}"

make -s || { echo "build failed"; exit 1; }
../../bin/kiln --emit-runtime -o /usr/local/lib/kiln/libkilnrt.so || { echo "emit-runtime failed"; exit 1; }

OUTDIR=/tmp/kiln_shared_check
rm -rf "$OUTDIR"
mkdir -p "$OUTDIR"

fail=0

for f in ../front/tests/cases/run/run_lang_j.ash ../front/tests/cases/run/run_oop_j.ash ../front/tests/cases/run/run_stdlib_j.ash ../../examples/*.ash; do
    name=$(basename "$f" .ash)
    static_out="$OUTDIR/${name}.static.out"
    shared_out="$OUTDIR/${name}.shared.out"
    ../../bin/kiln "$f" -o "$OUTDIR/${name}.static" --link=static > "$static_out" 2>&1
    ../../bin/kiln "$f" -o "$OUTDIR/${name}.shared" --link=shared > "$shared_out" 2>&1
    timeout 10 "$OUTDIR/${name}.static" >> "$static_out" 2>&1 < /dev/null
    timeout 10 "$OUTDIR/${name}.shared" >> "$shared_out" 2>&1 < /dev/null
    if ! diff -q "$static_out" "$shared_out" > /dev/null; then
        echo "MISMATCH: $name (static vs shared)"
        diff "$static_out" "$shared_out"
        fail=1
    fi
done

if [[ $fail -eq 0 ]]; then
    echo "shared_link_check: OK"
else
    exit 1
fi
