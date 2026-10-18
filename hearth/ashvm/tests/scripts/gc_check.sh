#!/usr/bin/env bash
# ashvm/tests/scripts/gc_check.sh
# Checks the collector (make GC=1) against the default VM: (1) every test program prints exactly
# the same when the collector runs at EVERY safe point (ASH_GC_STRESS=1), (2) a churn program needs
# far less memory, (3) the default build contains no collector code. bin/ashvm is rebuilt at the end,
# so it is left as `make @ashvm` would leave it (no collector).
set -uo pipefail
cd "$(dirname "$0")/../.." # -> ashvm/
# the optional standard library (ash-stdlib) is a sibling checkout unless ASH_PATH says otherwise
export ASH_PATH="${ASH_PATH:-$PWD/../../../ash-stdlib/src/main/ash}"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT

make clean >/dev/null 2>&1; make >/dev/null 2>&1 || { echo "default build failed"; exit 1; }
cp ../../bin/ashvm "$TMP/default"
make clean >/dev/null 2>&1; make GC=1 >/dev/null 2>&1 || { echo "GC build failed"; exit 1; }
cp ../../bin/ashvm "$TMP/gc"
make clean >/dev/null 2>&1; make >/dev/null 2>&1   # restore the default binary

fail=0
if nm "$TMP/default" | grep -q gc_collect; then echo "FAIL default build contains the collector"; fail=1; else echo "OK   default build has no collector"; fi

for f in tests/cases/*/*.ash ../front/tests/cases/run/*_j.ash ../../examples/*.ash; do
    d=$(dirname "$f")
    flags=(); case "$f" in *_wrap_*) flags=(--wrap-int);; esac
    a=$(cd "$d" && "$TMP/default" run "${flags[@]}" "$(basename "$f")" 2>&1; echo "rc=$?")
    b=$(cd "$d" && ASH_GC_STRESS=1 "$TMP/gc" run "${flags[@]}" "$(basename "$f")" 2>&1; echo "rc=$?")
    if [ "$a" == "$b" ]; then echo "OK   $(basename "$f") (gc stress)"; else echo "FAIL $(basename "$f") differs under GC stress"; fail=1; fi
done

peak() { python3 -c "import resource,subprocess,sys;subprocess.run(sys.argv[1:],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL);print(resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss//1024)" "$@"; }
pd=$(peak "$TMP/default" run tests/cases/memory/GcChurn.ash); pg=$(peak "$TMP/gc" run tests/cases/memory/GcChurn.ash)
if [ "$pg" -lt $((pd / 3)) ]; then echo "OK   GcChurn peak memory: default ${pd} MB, collector ${pg} MB"; else echo "FAIL GcChurn memory: default ${pd} MB, collector ${pg} MB"; fail=1; fi
exit $fail
