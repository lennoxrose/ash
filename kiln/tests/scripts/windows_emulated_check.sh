#!/usr/bin/env bash
# kiln/tests/scripts/windows_emulated_check.sh
# No Windows machine needed: builds every kiln test case as a Windows PE (--target=windows),
# runs it under the Unicorn x86-64 emulator (run_pe_emulated.py, which fakes kernel32 on top of the
# real filesystem), and compares its output with the same program built for Linux.
# Needs python3 with `unicorn` and `pefile`; a venv is created in /tmp/kiln_winemu on first use.
set -uo pipefail
cd "$(dirname "$0")/../.." # -> kiln/
KILN="$PWD/../bin/kiln"
SCRIPTS="$PWD/tests/scripts"
VENV=/tmp/kiln_winemu
if [ ! -x "$VENV/bin/python" ]; then
    python3 -m venv "$VENV" && "$VENV/bin/pip" install -q unicorn pefile || { echo "could not set up the emulator venv"; exit 1; }
fi
make -s || { echo "build failed"; exit 1; }
OUT=/tmp/kiln_winemu_out; mkdir -p "$OUT"
pass=0; fail=0
for f in tests/cases/*/*.ash; do
    n=$(basename "$f" .ash); d=$(dirname "$f")
    case $n in argv|input*|*circular*|*collision*) continue;; esac # need real arguments / stdin / expected failure
    rm -rf /tmp/ash_fs_test_dir
    (cd "$d" && "$KILN" "$n.ash" -o "$OUT/lin_$n" >/dev/null 2>&1 && "$KILN" "$n.ash" -o "$OUT/win_$n.exe" --target=windows >/dev/null 2>&1) || { echo "COMPILE-FAIL: $n"; fail=$((fail + 1)); continue; }
    (cd "$d" && timeout 120 "$OUT/lin_$n" < /dev/null > "$OUT/lin_$n.out" 2>&1)
    rm -rf /tmp/ash_fs_test_dir
    (cd "$d" && timeout 600 "$VENV/bin/python" "$SCRIPTS/run_pe_emulated.py" "$OUT/win_$n.exe" < /dev/null > "$OUT/win_$n.out" 2>&1)
    if cmp -s "$OUT/lin_$n.out" "$OUT/win_$n.out"; then pass=$((pass + 1)); else fail=$((fail + 1)); echo "DIFF: $n"; fi
done
echo "windows (emulated) vs linux: same $pass, different $fail"
[ "$fail" -eq 0 ]
