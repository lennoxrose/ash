#!/usr/bin/env bash
# Type-checker regression: ok_*_j.ash must check clean, err_*_j.ash must fail and
# print every "// expect: <text>" line of the file.
set -u
cd "$(dirname "$0")/../.."
BIN=../../bin/ashfront
dir=tests/cases/types
fail=0
for f in "$dir"/ok_*_j.ash; do
    if ! out=$($BIN --check "$f" 2>&1); then
        echo "FAIL $f (expected clean)"; echo "$out"; fail=1
    else echo "ok   $f"; fi
done
for f in "$dir"/err_*_j.ash; do
    out=$($BIN --check "$f" 2>&1); rc=$?
    if [ $rc -eq 0 ]; then echo "FAIL $f (expected errors)"; fail=1; continue; fi
    miss=0
    while IFS= read -r want; do
        if ! grep -qF -- "$want" <<<"$out"; then echo "FAIL $f: missing '$want'"; miss=1; fi
    done < <(sed -n 's#^// expect: ##p' "$f")
    if [ $miss -eq 1 ]; then echo "$out"; fail=1; else echo "ok   $f"; fi
done
for f in tests/cases/syntax/*.ash ../../main.ash; do
    if $BIN --parse "$f" >/dev/null 2>&1; then echo "ok   $f (parses)"; else echo "FAIL $f (parse)"; fail=1; fi
done
exit $fail
