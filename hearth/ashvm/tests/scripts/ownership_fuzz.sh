#!/usr/bin/env bash
# ashvm/tests/scripts/ownership_fuzz.sh [first_seed] [last_seed]
# Differential test of the ownership pass and the freeing it enables: for each seed a random program is
# generated (ownership_fuzz_gen.py), run with memory-safety on and off, and the outputs must be identical.
# Programs the move checker rejects in "on" mode are skipped (the generator may reuse a moved value on purpose).
# With ASAN=/path/to/ashvm-built-with-address-sanitizer every accepted program also runs under it, and with
# KILN=1 it is compiled with kiln too and its output must match as well. Programs are tiny; each run is capped.
set -uo pipefail
cd "$(dirname "$0")/../.." # -> ashvm/
export ASH_PATH="${ASH_PATH:-$PWD/../../../ash-stdlib/src/main/ash}"
make -s || { echo "build failed"; exit 1; }
VM="$PWD/../../bin/ashvm"; KILN_BIN="$PWD/../../bin/kiln"; GEN="$PWD/tests/scripts/ownership_fuzz_gen.py"
FIRST="${1:-1}"; LAST="${2:-200}"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
mkdir "$TMP/on" "$TMP/off"
printf 'name: f\nversion: 0.1.0\nmemory-safety: on\n' > "$TMP/on/ash.pkg"
printf 'name: f\nversion: 0.1.0\nmemory-safety: off\n' > "$TMP/off/ash.pkg"
ulimit -v 4000000
ok=0; skipped=0; bad=0
for seed in $(seq "$FIRST" "$LAST"); do
    python3 "$GEN" "$seed" > "$TMP/on/F$seed.ash"
    cp "$TMP/on/F$seed.ash" "$TMP/off/"
    on_out=$("$VM" run "$TMP/on/F$seed.ash" 2>&1)
    if echo "$on_out" | grep -q '^error'; then skipped=$((skipped + 1)); continue; fi
    off_out=$("$VM" run "$TMP/off/F$seed.ash" 2>&1)
    fail=""
    [ "$on_out" == "$off_out" ] || fail="on/off differ"
    if [ -n "${ASAN:-}" ] && ASAN_OPTIONS=detect_leaks=0 timeout 60 "$ASAN" run "$TMP/on/F$seed.ash" 2>&1 | grep -q AddressSanitizer; then fail="$fail asan"; fi
    if [ -n "${KILN:-}" ]; then
        if "$KILN_BIN" "$TMP/on/F$seed.ash" -o "$TMP/k$seed" > /dev/null 2>&1; then
            k_out=$(timeout 30 "$TMP/k$seed" 2>&1); rm -f "$TMP/k$seed"
            [ "$k_out" == "$on_out" ] || fail="$fail kiln differs"
        else fail="$fail kiln compile"; fi
    fi
    if [ -n "$fail" ]; then echo "FAIL seed $seed:$fail"; cp "$TMP/on/F$seed.ash" "/tmp/ownership_fuzz_fail_$seed.ash"; bad=$((bad + 1)); else ok=$((ok + 1)); fi
done
echo "ownership fuzz: ok=$ok skipped=$skipped bad=$bad"
[ "$bad" -eq 0 ]
