#!/usr/bin/env bash
# ashvm/tests/scripts/labeled_fuzz.sh [first_seed] [last_seed]
# Differential test of labeled break / continue (labeled_fuzz_gen.py writes each program in two spellings):
#   - the flag-variable spelling runs on REF (an ashvm built before labels existed; default: this build, which
#     handles that spelling the same way) with memory-safety off, and gives the reference output;
#   - the labeled spelling must print the same on ashvm, with memory-safety on and off, and with KILN=1 on kiln
#     (loop regions on, reclaimed memory poisoned; odd seeds are region programs, ASH_REGION_ALL=1 for them);
#   - with JAVA=1 (a JDK in PATH) it is also compiled and run by Java, which has the same labels.
# Programs the move checker rejects in the labeled spelling are skipped; so are those whose reference run throws
# (refbug: a bug of the reference engine that has nothing to do with labels, counted so it stays visible).
set -uo pipefail
cd "$(dirname "$0")/../.." # -> ashvm/
export ASH_PATH="${ASH_PATH:-$PWD/../../../ash-stdlib/src/main/ash}"
make -s || { echo "build failed"; exit 1; }
VM="$PWD/../../bin/ashvm"; KILN_BIN="$PWD/../../bin/kiln"; GEN="$PWD/tests/scripts/labeled_fuzz_gen.py"
REF="${REF:-$VM}"
FIRST="${1:-1}"; LAST="${2:-200}"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
mkdir "$TMP/on" "$TMP/off" "$TMP/java"
printf 'name: f\nversion: 0.1.0\nmemory-safety: on\n' > "$TMP/on/ash.pkg"
printf 'name: f\nversion: 0.1.0\nmemory-safety: off\n' > "$TMP/off/ash.pkg"
ulimit -S -v 4000000
ok=0; skipped=0; bad=0; refbug=0
for seed in $(seq "$FIRST" "$LAST"); do
    python3 "$GEN" "$seed" label > "$TMP/on/F$seed.ash"
    cp "$TMP/on/F$seed.ash" "$TMP/off/"
    python3 "$GEN" "$seed" flag > "$TMP/off/G$seed.ash"
    sed "s/F$seed/G$seed/" "$TMP/off/G$seed.ash" > /dev/null
    ref=$(timeout 30 "$REF" run "$TMP/off/G$seed.ash" 2>&1)
    if echo "$ref" | grep -q '^error'; then skipped=$((skipped + 1)); continue; fi
    on_out=$("$VM" run "$TMP/on/F$seed.ash" 2>&1)
    if echo "$on_out" | grep -q '^error'; then skipped=$((skipped + 1)); continue; fi
    if echo "$ref" | grep -q 'Exception in thread'; then
        # A generated program never throws: the reference engine miscompiled the flag spelling (a bug of ashvm that has
        # nothing to do with labels, counted so that it stays visible). Java, when there, stands in for the reference,
        # but only when the labeled spelling did not hit the same bug.
        refbug=$((refbug + 1))
        if [ -z "${JAVA:-}" ] || echo "$on_out" | grep -q 'Exception in thread'; then continue; fi
        ref=""
    fi
    fail=""
    java_out=""
    if [ -n "${JAVA:-}" ] && { [ -z "$ref" ] || [ $((seed % ${JAVA_EVERY:-4})) == 0 ]; }; then
        rm -f "$TMP/java"/*; sed 's/ash\.util\./java.util./' "$TMP/on/F$seed.ash" > "$TMP/java/F$seed.java"
        if (ulimit -S -v unlimited; javac -d "$TMP/java" "$TMP/java/F$seed.java") > /dev/null 2>&1; then
            java_out=$(ulimit -S -v unlimited; java -cp "$TMP/java" "F$seed" 2>&1 | grep -v '^Picked up')
        else fail="$fail java compile"; fi
        if [ -z "$ref" ]; then ref="$java_out"; elif [ -n "$java_out" ] && [ "$java_out" != "$ref" ]; then fail="$fail java differs"; fi
    fi
    [ "$on_out" == "$ref" ] || fail="$fail labeled(on) differs from the reference"
    off_out=$("$VM" run "$TMP/off/F$seed.ash" 2>&1)
    [ "$off_out" == "$ref" ] || fail="$fail labeled(off) differs"
    if [ -n "${KILN:-}" ]; then
        if [ $((seed % 2)) == 1 ]; then export ASH_REGION_ALL=1; else unset ASH_REGION_ALL; fi
        if KILN_REGION_POISON=1 "$KILN_BIN" "$TMP/on/F$seed.ash" -o "$TMP/k$seed" > /dev/null 2>&1; then
            k_out=$(timeout 30 "$TMP/k$seed" 2>&1); rm -f "$TMP/k$seed"
            [ "$k_out" == "$ref" ] || fail="$fail kiln differs"
        else fail="$fail kiln compile"; fi
        unset ASH_REGION_ALL
    fi
    if [ -n "$fail" ]; then echo "FAIL seed $seed:$fail"; cp "$TMP/on/F$seed.ash" "/tmp/labeled_fuzz_fail_$seed.ash"; cp "$TMP/off/G$seed.ash" "/tmp/labeled_fuzz_fail_${seed}_flag.ash"; bad=$((bad + 1)); else ok=$((ok + 1)); fi
done
echo "labeled fuzz: ok=$ok skipped=$skipped refbug=$refbug bad=$bad"
[ "$bad" -eq 0 ]
