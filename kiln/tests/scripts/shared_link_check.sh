#!/usr/bin/env bash
# kiln/tests/scripts/shared_link_check.sh
#
# --link=shared had NO automated coverage at all before this (run_regression.sh
# only ever compiles with kiln's default --link=static) -- this fills that
# gap generally, and specifically covers matrix_mul (codegen/C/collections/matrix_mul.c,
# elf/H/elf_dynamic.h's second DT_NEEDED for libpyre.so), the one builtin
# that only exists under --link=shared at all.
#
# 1. Rebuilds kiln, libkilnrt.so, and libpyre.so, installing both at
#    kiln's hardcoded rpath (KILN_RPATH, elf/H/elf_dynamic.h) -- there is
#    no env-var override, so this writes to /usr/local/lib/kiln for real,
#    same as doing it by hand.
# 2. Compiles a sample of ordinary test cases BOTH --link=static and
#    --link=shared and diffs their output -- proving the two modes agree
#    wherever both can run a program, which also means the second
#    DT_NEEDED and the combined GOT/hash/dynsym table didn't disturb
#    kiln's own existing runtime imports (heap_alloc, print_top, ...).
# 3. Compiles and runs a small matrix_mul program (inline here, not under
#    tests/cases/ -- run_regression.sh's tests/cases/*/*.ash glob would
#    pick it up and "COMPILE FAIL" with kiln's default --link=static,
#    which is correct behavior but not what that suite is for) under
#    --link=shared, and separately confirms --link=static rejects it with
#    a clear compile error instead of silently ignoring it.
set -uo pipefail
cd "$(dirname "$0")/../.." # -> kiln/

make -s || { echo "build failed"; exit 1; }
../bin/kiln --emit-runtime -o /usr/local/lib/kiln/libkilnrt.so || { echo "emit-runtime failed"; exit 1; }
(cd ../pyre && make -s lib) || { echo "pyre build failed"; exit 1; }

OUTDIR=/tmp/kiln_shared_check
rm -rf "$OUTDIR"
mkdir -p "$OUTDIR"

fail=0

for f in tests/cases/basics/*.ash tests/cases/collections/*.ash tests/cases/strings/*.ash; do
    name=$(basename "$f" .ash)
    static_out="$OUTDIR/${name}.static.out"
    shared_out="$OUTDIR/${name}.shared.out"
    ../bin/kiln "$f" -o "$OUTDIR/${name}.static" --link=static > "$static_out" 2>&1
    ../bin/kiln "$f" -o "$OUTDIR/${name}.shared" --link=shared > "$shared_out" 2>&1
    if [[ "$name" == "input_builtin" ]]; then
        printf 'World\n5\n' | timeout 10 "$OUTDIR/${name}.static" >> "$static_out" 2>&1
        printf 'World\n5\n' | timeout 10 "$OUTDIR/${name}.shared" >> "$shared_out" 2>&1
    else
        timeout 10 "$OUTDIR/${name}.static" >> "$static_out" 2>&1
        timeout 10 "$OUTDIR/${name}.shared" >> "$shared_out" 2>&1
    fi
    if ! diff -q "$static_out" "$shared_out" > /dev/null; then
        echo "MISMATCH: $name (static vs shared)"
        diff "$static_out" "$shared_out"
        fail=1
    fi
done

cat > "$OUTDIR/matrix_mul_shared.ash" << 'EOF'
local a = [[1, 2], [3, 4]];
local b = [[5, 6], [7, 8]];
say matrix_mul(a, b);
EOF

../bin/kiln "$OUTDIR/matrix_mul_shared.ash" -o "$OUTDIR/matrix_mul_shared.bin" --link=shared > "$OUTDIR/matrix_mul_shared.out" 2>&1
timeout 10 "$OUTDIR/matrix_mul_shared.bin" >> "$OUTDIR/matrix_mul_shared.out" 2>&1
if ! grep -qF '[[19, 22], [43, 50]]' "$OUTDIR/matrix_mul_shared.out"; then
    echo "MISMATCH: matrix_mul under --link=shared did not produce the expected result"
    cat "$OUTDIR/matrix_mul_shared.out"
    fail=1
fi

if ../bin/kiln "$OUTDIR/matrix_mul_shared.ash" -o "$OUTDIR/matrix_mul_static.bin" --link=static > "$OUTDIR/matrix_mul_static_reject.out" 2>&1; then
    echo "MISMATCH: matrix_mul under --link=static should have been rejected at compile time, but compiled"
    fail=1
elif ! grep -q "requires --link=shared" "$OUTDIR/matrix_mul_static_reject.out"; then
    echo "MISMATCH: matrix_mul --link=static rejection message changed unexpectedly"
    cat "$OUTDIR/matrix_mul_static_reject.out"
    fail=1
fi

if [[ $fail -eq 0 ]]; then
    echo "shared_link_check: OK"
else
    exit 1
fi
