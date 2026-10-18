#!/usr/bin/env bash
# Runtime regression. Every tests/cases/run/*_j.ash and every ../../examples/*.ash is lowered and
# executed on ashvm, and its stdout compared byte-for-byte with the matching .out file.
# Examples are additionally compiled to a native binary with kiln and compared again.
set -u
cd "$(dirname "$0")/../.."
BIN=../../bin/ashfront
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
fail=0
check() { # name act exp
    if [ "$2" == "$(cat "$3")" ]; then echo "ok   $1"; else echo "FAIL $1"; diff <(echo "$2") "$3" | head -20; fail=1; fi
}
for f in tests/cases/run/*_j.ash; do
    rm -rf /tmp/ash_stdlib_test_dir
    flags=""
    case "$f" in *_wrap_*) flags="--wrap-int";; esac   # *_wrap_* tests need Java 32-bit int semantics
    act=$($BIN --run $flags "$f" 2>&1); check "$f (ashvm $flags)" "$act" "${f%.ash}.out"
done
for f in ../../examples/*.ash; do
    n=$(basename "$f" .ash)
    rm -rf /tmp/ash_example_io
    act=$($BIN --run "$f" 2>&1); check "examples/$n (ashvm)" "$act" "${f%.ash}.out"
    if [ -x ../../bin/kiln ]; then
        rm -rf /tmp/ash_example_io
        if $BIN --build "$f" -o "$TMP/$n" >"$TMP/$n.err" 2>&1; then
            act=$("$TMP/$n" 2>&1); check "examples/$n (kiln native)" "$act" "${f%.ash}.out"
        else echo "FAIL examples/$n (kiln build)"; head -5 "$TMP/$n.err"; fail=1; fi
    fi
done
exit $fail
