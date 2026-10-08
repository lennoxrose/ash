#!/usr/bin/env bash
# kiln/tests/scripts/windows_wine_check.sh
# Builds every kiln test case as a Windows PE (--target=windows), runs it under Wine -- whose
# kernel32 is an independent reimplementation of the Windows API -- and compares its output with
# the same program built for Linux.
#   WINE=/path/to/wine kiln/tests/scripts/windows_wine_check.sh
# Without WINE it uses `wine` from PATH. A portable build works without root, e.g.
# https://github.com/Kron4ek/Wine-Builds (unpack the amd64 tarball, point WINE at bin/wine).
set -uo pipefail
cd "$(dirname "$0")/../.." # -> kiln/
KILN="$PWD/../bin/kiln"
WINE="${WINE:-wine}"
command -v "$WINE" >/dev/null 2>&1 || [ -x "$WINE" ] || { echo "wine not found (set WINE=/path/to/wine)"; exit 1; }
export WINEPREFIX="${WINEPREFIX:-$HOME/.cache/ash_wine_prefix}" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" DISPLAY=
make -s || { echo "build failed"; exit 1; }
OUT=/tmp/kiln_wine_out; mkdir -p "$OUT"
pass=0; fail=0
for f in tests/cases/*/*.ash; do
    n=$(basename "$f" .ash); d=$(dirname "$f")
    case $n in argv|input*|*circular*|*collision*) continue;; esac # need real arguments / stdin / expected failure
    rm -rf /tmp/ash_fs_test_dir
    (cd "$d" && "$KILN" "$n.ash" -o "$OUT/lin_$n" >/dev/null 2>&1 && "$KILN" "$n.ash" -o "$OUT/win_$n.exe" --target=windows >/dev/null 2>&1) || { echo "COMPILE-FAIL: $n"; fail=$((fail + 1)); continue; }
    (cd "$d" && timeout 120 "$OUT/lin_$n" < /dev/null > "$OUT/lin_$n.out" 2>&1)
    rm -rf /tmp/ash_fs_test_dir
    (cd "$d" && timeout 300 "$WINE" "$OUT/win_$n.exe" < /dev/null > "$OUT/win_$n.out" 2>/dev/null)
    if cmp -s "$OUT/lin_$n.out" "$OUT/win_$n.out"; then pass=$((pass + 1)); else fail=$((fail + 1)); echo "DIFF: $n"; fi
done
echo "windows (wine) vs linux: same $pass, different $fail"
[ "$fail" -eq 0 ]
