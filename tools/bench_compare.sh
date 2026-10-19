#!/bin/sh
# Performance gate: Ash must never get slower. Compares two ashvm binaries on hearth/ashvm/benchmarks
# and fails if the new one is slower than the baseline on any benchmark by more than the tolerance.
#
#   tools/bench_compare.sh <baseline-ashvm> <new-ashvm> [runs=7] [tolerance-percent=2]
#
# Typical use, from a checkout of your branch:
#   git stash -q 2>/dev/null; git checkout -q main && make @ashvm && cp bin/ashvm /tmp/ashvm_base
#   git checkout -q - && make @ashvm && tools/bench_compare.sh /tmp/ashvm_base bin/ashvm
#
# Each program runs <runs> times per binary, alternating, and the best (minimum) time counts, which
# filters out machine noise. The default 2% tolerance is the measured link-layout noise of identical code
# (see CHANGELOG, "relinking the same ashvm objects ... moves single benchmarks by about 1.5%").
# A real regression is always a few percent or more; if the tool reports one, fix the code, not the number.
set -eu
[ $# -ge 2 ] || { sed -n '2,6p' "$0" | sed 's/^# \{0,1\}//'; exit 2; }
BASE=$1; NEW=$2; RUNS=${3:-7}; TOL=${4:-2}
cd "$(dirname "$0")/.."
[ -x "$BASE" ] && [ -x "$NEW" ] || { echo "bench_compare: both ashvm binaries must exist and be executable" >&2; exit 2; }
# the optional standard library is a sibling checkout unless ASH_PATH says otherwise (the hashmap benchmark needs it)
export ASH_PATH="${ASH_PATH:-$PWD/../ash-stdlib/src/main/ash}"

now() { date +%s%N; }
fail=0
printf '%-16s %10s %10s %8s\n' benchmark base_s new_s ratio
for f in hearth/ashvm/benchmarks/*.ash; do
    n=$(basename "$f" .ash)
    "$BASE" run "$f" >/dev/null 2>&1 || { echo "bench_compare: baseline fails on $n; skipping" >&2; continue; }
    "$NEW" run "$f" >/dev/null 2>&1 || { echo "bench_compare: NEW BINARY FAILS on $n" >&2; fail=1; continue; }
    bb=""; nb=""; i=0
    while [ "$i" -lt "$RUNS" ]; do
        s=$(now); "$BASE" run "$f" >/dev/null 2>&1; e=$(now); bb="$bb $((e - s))"
        s=$(now); "$NEW"  run "$f" >/dev/null 2>&1; e=$(now); nb="$nb $((e - s))"
        i=$((i + 1))
    done
    line=$(printf '%s\n%s\n' "$bb" "$nb" | awk -v n="$n" -v tol="$TOL" '
        NR == 1 { b = $1; for (i = 2; i <= NF; i++) if ($i < b) b = $i }
        NR == 2 { w = $1; for (i = 2; i <= NF; i++) if ($i < w) w = $i }
        END { r = w / b; printf "%-16s %10.4f %10.4f %8.3f%s\n", n, b / 1e9, w / 1e9, r, (r > 1 + tol / 100) ? "  SLOWER" : "" }')
    echo "$line"
    case $line in *SLOWER) fail=1 ;; esac
done
if [ "$fail" = 0 ]; then echo "OK: no benchmark slower than the baseline by more than ${TOL}%"; else echo "FAIL: performance regression (or the new binary fails); see above" >&2; fi
exit $fail
