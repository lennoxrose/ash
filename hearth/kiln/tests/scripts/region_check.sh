#!/usr/bin/env bash
# Loop regions (front/src/jown/region.c, kiln only): a loop whose body keeps nothing it allocates past the
# iteration gets its heap rewound after every iteration.
#   1. RegionLoops.ash: every loop in it that should be a region is one (counted with ASH_REGION_TRACE), and the
#      program prints what ashvm prints even when the reclaimed bytes are overwritten with 0xDD
#      (KILN_REGION_POISON), so a value that was used after its iteration would show up as garbage.
#   2. RegionEscapes.ash: loops that let a value escape (outer variable, field, outer array or list, a call that
#      keeps its argument, a lambda, ...) are not regions, and print the right thing with poison on.
#   3. The same programs without regions (KILN_NO_REGIONS=1) print the same, and a loop of temporaries runs in a
#      few MB with regions against a hundred or more without.
#   4. Every other program of the regression suite also passes with poison on.
set -uo pipefail
ulimit -v 3000000
cd "$(dirname "$0")/../.." # -> kiln/
export ASH_PATH="${ASH_PATH:-$PWD/../../../ash-stdlib/src/main/ash}"
make -s || { echo "build failed"; exit 1; }
KILN=../../bin/kiln
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
fail=0

check() { # name expected_region_loops
    local f="tests/cases/ownership/$1.ash"
    local n
    n=$(ASH_REGION_TRACE=1 "$KILN" "$f" -o "$TMP/$1" 2>&1 | grep -c '^region loop:')
    [ "$n" == "$2" ] || { echo "FAIL: $1 has $n region loops, expected $2"; fail=1; }
    KILN_REGION_POISON=1 "$KILN" "$f" -o "$TMP/$1.poison" > "$TMP/$1.err" 2>&1 || { echo "FAIL: $1 does not compile"; head -5 "$TMP/$1.err"; fail=1; return; }
    "$TMP/$1.poison" > "$TMP/$1.out" 2>&1 < /dev/null
    cmp -s "$TMP/$1.out" "${f%.ash}.out" || { echo "FAIL: $1 with poison differs"; diff "$TMP/$1.out" "${f%.ash}.out" | head -5; fail=1; }
    KILN_NO_REGIONS=1 "$KILN" "$f" -o "$TMP/$1.plain" > /dev/null 2>&1
    "$TMP/$1.plain" > "$TMP/$1.plain.out" 2>&1 < /dev/null
    cmp -s "$TMP/$1.plain.out" "${f%.ash}.out" || { echo "FAIL: $1 without regions differs"; fail=1; }
    echo "ok: $1 ($n region loops)"
}
check RegionLoops 12
check RegionEscapes 0
check RegionForEach 16
check RegionForEachLocal 8
check RegionMaps 7
check RegionMapEscapes 0
check RegionForEachEscapes 0
check RegionCalls 12
check RegionCallEscapes 0
check RegionTry 9
check RegionTryEscapes 0

mkdir "$TMP/p"
printf 'name: m\nversion: 0.1.0\nmemory-safety: on\n' > "$TMP/p/ash.pkg"
cat > "$TMP/p/Temps.ash" <<'JAVA'
public class Temps {
    public static void main(String[] args) {
        long total = 0;
        for (int i = 0; i < 2000000; i++) {
            String s = "item-" + i;
            total += (s + "!" + i).length();
            total += s.substring(2).length();
            int[] xs = new int[8];
            xs[3] = i;
            total += xs[3] + xs.length;
        }
        System.out.println(total);
    }
}
JAVA
peak() { python3 - "$@" <<'PY'
import resource, subprocess, sys
r = subprocess.run(sys.argv[1:], capture_output=True, text=True)
print(resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss // 1024, r.stdout.strip().splitlines()[-1] if r.stdout.strip() else r.stderr[:200])
PY
}
"$KILN" "$TMP/p/Temps.ash" -o "$TMP/p/with" && KILN_NO_REGIONS=1 "$KILN" "$TMP/p/Temps.ash" -o "$TMP/p/without" || { echo "compile failed"; exit 1; }
on=$(peak "$TMP/p/with"); off=$(peak "$TMP/p/without")
echo "regions: ${on}  without: ${off}  (peak MB, last line of output)"
[ "${on#* }" == "${off#* }" ] || { echo "FAIL: outputs differ"; fail=1; }
[ "${on%% *}" -le 16 ] || { echo "FAIL: peak with regions is ${on%% *} MB"; fail=1; }
[ "${off%% *}" -ge $(( ${on%% *} * 4 )) ] || { echo "FAIL: without regions is not clearly larger"; fail=1; }

cat > "$TMP/p/Calls.ash" <<'JAVA'
public class Calls {
    static int width(String s) {
        String padded = "[" + s + "]";
        int[] junk = new int[16];
        junk[3] = padded.length();
        return junk[3];
    }
    public static void main(String[] args) {
        long total = 0;
        for (int i = 0; i < 2000000; i++) total += width("item-" + i);
        System.out.println(total);
    }
}
JAVA
"$KILN" "$TMP/p/Calls.ash" -o "$TMP/p/cwith" && KILN_NO_REGIONS=1 "$KILN" "$TMP/p/Calls.ash" -o "$TMP/p/cwithout" || { echo "compile failed"; exit 1; }
on=$(peak "$TMP/p/cwith"); off=$(peak "$TMP/p/cwithout")
echo "calls: regions: ${on}  without: ${off}  (peak MB, last line of output)"
[ "${on#* }" == "${off#* }" ] || { echo "FAIL: call outputs differ"; fail=1; }
[ "${on%% *}" -le 16 ] || { echo "FAIL: peak with regions is ${on%% *} MB"; fail=1; }
[ "${off%% *}" -ge $(( ${on%% *} * 3 )) ] || { echo "FAIL: without regions is not clearly larger"; fail=1; }

cat > "$TMP/p/Maps.ash" <<'JAVA'
import ash.util.HashMap;
import ash.util.HashSet;
import ash.util.Map;
import ash.util.Set;

public class Maps {
    public static void main(String[] args) {
        long total = 0;
        for (int i = 0; i < 300000; i++) {
            Map<String, Integer> m = new HashMap<>();
            Set<Integer> seen = new HashSet<>();
            for (int j = 0; j < 6; j++) {
                String k = "k" + (j % 4) + i;
                Integer? c = m.get(k);
                m.put(k, c == null ? 1 : c + 1);
                seen.add(j % 3);
            }
            total += m.size() * 10 + seen.size();
        }
        System.out.println(total);
    }
}
JAVA
"$KILN" "$TMP/p/Maps.ash" -o "$TMP/p/mwith" && KILN_NO_REGIONS=1 "$KILN" "$TMP/p/Maps.ash" -o "$TMP/p/mwithout" || { echo "compile failed"; exit 1; }
on=$(peak "$TMP/p/mwith"); off=$(peak "$TMP/p/mwithout")
echo "maps: regions: ${on}  without: ${off}  (peak MB, last line of output)"
[ "${on#* }" == "${off#* }" ] || { echo "FAIL: map outputs differ"; fail=1; }
[ "${on%% *}" -le 16 ] || { echo "FAIL: peak with regions is ${on%% *} MB"; fail=1; }
[ "${off%% *}" -ge $(( ${on%% *} * 3 )) ] || { echo "FAIL: without regions is not clearly larger"; fail=1; }

for f in tests/cases/*/*.ash ../front/tests/cases/run/*_j.ash ../../examples/*.ash; do
    n=$(basename "$f" .ash)
    [ "$n" == run_double_j ] && continue
    flags=()
    case "$f" in *_wrap_*) flags=(--wrap-int);; esac
    KILN_REGION_POISON=1 "$KILN" "$f" -o "$TMP/$n" "${flags[@]}" > /dev/null 2>&1 || { echo "FAIL: $f does not compile with poison"; fail=1; continue; }
    rm -rf /tmp/ash_stdlib_test_dir /tmp/ash_example_io
    "$TMP/$n" > "$TMP/$n.out" 2>&1 < /dev/null
    cmp -s "$TMP/$n.out" "${f%.ash}.out" || { echo "FAIL: $f differs with poison"; fail=1; }
done
[ "$fail" == 0 ] && echo "region check ok"
exit $fail
