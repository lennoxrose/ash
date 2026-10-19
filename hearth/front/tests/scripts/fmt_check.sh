#!/usr/bin/env bash
# `ashvm fmt`: tests/cases/fmt/Name.ash must format to exactly Name.fmt; then, for every Ash file of the repository
# (tests, examples, benchmarks) and of the standard library: the formatter accepts it (it compares the token streams
# of input and output itself), formatting twice gives the same text, and an ok_*_j.ash case still checks clean
# after formatting.
set -u
cd "$(dirname "$0")/../.."
export ASH_PATH="${ASH_PATH:-$PWD/../../../ash-stdlib/src/main/ash}"
VM=../../bin/ashvm
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
fail=0
for f in tests/cases/fmt/*.ash; do
    "$VM" fmt "$f" > "$TMP/out" 2>&1
    if cmp -s "$TMP/out" "${f%.ash}.fmt"; then echo "ok   $f"; else echo "DIFF $f"; diff "$TMP/out" "${f%.ash}.fmt" | head -10; fail=1; fi
done
n=0
while IFS= read -r f; do
    n=$((n + 1))
    if ! "$VM" fmt "$f" > "$TMP/once.ash" 2> "$TMP/err"; then echo "FAIL $f: $(head -1 "$TMP/err")"; fail=1; continue; fi
    if ! "$VM" fmt "$TMP/once.ash" > "$TMP/twice.ash" 2> "$TMP/err" || ! cmp -s "$TMP/once.ash" "$TMP/twice.ash"; then echo "FAIL $f: not idempotent"; fail=1; fi
    case "$f" in */types/ok_*_j.ash)
        cp "$TMP/once.ash" "$TMP/$(basename "$f")"
        "$VM" check "$TMP/$(basename "$f")" > /dev/null 2>&1 || { echo "FAIL $f: does not check after formatting"; fail=1; } ;;
    esac
done < <(find tests/cases ../ashvm/tests/cases ../kiln/tests/cases ../ashvm/benchmarks ../../examples "$ASH_PATH" -name '*.ash' -not -path '*/fmt/*' 2>/dev/null | sort)
[ $fail -eq 0 ] && echo "fmt check ok ($n files)"
exit $fail
