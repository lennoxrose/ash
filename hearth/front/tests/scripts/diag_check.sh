#!/usr/bin/env bash
# Diagnostics regression on BOTH engines: every tests/cases/diag/Name.ash must fail to compile, and
#   - `ashvm check` and `kiln` must print exactly Name.diag on stderr (the text, notes and help lines included),
#   - `ashvm check --json` and `kiln --json` must print exactly Name.json on stdout (when the file exists).
# Regenerate an expectation with: UPDATE=1 hearth/front/tests/scripts/diag_check.sh (then read the diff).
set -u
cd "$(dirname "$0")/../.."
export ASH_PATH="${ASH_PATH:-$PWD/../../../ash-stdlib/src/main/ash}"
export NO_COLOR=1
VM=../../bin/ashvm
KILN=../../bin/kiln
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
fail=0
for f in tests/cases/diag/*.ash; do
    base=${f%.ash}
    "$VM" check "$f" > "$TMP/vm.out" 2> "$TMP/vm.err"; rc=$?
    if [ $rc -eq 0 ]; then echo "FAIL $f (expected errors)"; fail=1; continue; fi
    "$KILN" "$f" -o "$TMP/k.bin" > "$TMP/k.out" 2> "$TMP/k.err"; krc=$?
    if [ $krc -eq 0 ]; then echo "FAIL $f (kiln compiled it)"; fail=1; continue; fi
    if [ -n "${UPDATE:-}" ]; then cp "$TMP/vm.err" "$base.diag"; fi
    before=$fail
    for e in vm k; do
        if cmp -s "$TMP/$e.err" "$base.diag"; then :; else echo "DIFF $f ($e text)"; diff "$TMP/$e.err" "$base.diag" | head -10; fail=1; fi
    done
    if [ -n "${UPDATE:-}" ] && [ -f "$base.json" ]; then "$VM" check --json "$f" > "$base.json" 2>/dev/null; fi
    if [ -f "$base.json" ]; then
        "$VM" check --json "$f" > "$TMP/vm.json" 2> "$TMP/vm.jerr"
        "$KILN" "$f" -o "$TMP/k.bin" --json > "$TMP/k.json" 2> "$TMP/k.jerr"
        for e in vm k; do
            if cmp -s "$TMP/$e.json" "$base.json"; then :; else echo "DIFF $f ($e json)"; diff "$TMP/$e.json" "$base.json" | head -10; fail=1; fi
            if [ -s "$TMP/$e.jerr" ]; then echo "FAIL $f ($e wrote to stderr in json mode)"; head -3 "$TMP/$e.jerr"; fail=1; fi
        done
        if command -v python3 >/dev/null && ! python3 -c 'import json,sys; json.load(open(sys.argv[1]))' "$base.json"; then echo "FAIL $f (json does not parse)"; fail=1; fi
    fi
    [ $fail -eq $before ] && echo "ok   $f"
done
exit $fail
