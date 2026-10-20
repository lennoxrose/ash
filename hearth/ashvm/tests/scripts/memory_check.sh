#!/usr/bin/env bash
# Memory stays bounded: a loop that builds three million short-lived strings and then 300 thousand arrays and 100 thousand 8x16 grids must finish in a
# few MB with memory-safety on, and the same loop with it off keeps everything (hundreds of MB), which shows
# the check is measuring the freeing and not just a small program.
set -uo pipefail
# Hard cap on every program this script starts (virtual memory, 3 GB): the off-mode run keeps everything
# on purpose, and a bug must not be able to take the machine down with it.
ulimit -v 3000000
cd "$(dirname "$0")/../.." # -> project root
export ASH_PATH="${ASH_PATH:-$PWD/../../../ash-stdlib/src/main/ash}"
make -s || { echo "build failed"; exit 1; }
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
mkdir "$TMP/on" "$TMP/off"
printf 'name: m\nversion: 0.1.0\nmemory-safety: on\n' > "$TMP/on/ash.pkg"
printf 'name: m\nversion: 0.1.0\nmemory-safety: off\n' > "$TMP/off/ash.pkg"
for d in on off; do
cat > "$TMP/$d/Churn.ash" <<'JAVA'
public class Churn {
    public static void main(String[] args) {
        long total = 0;
        for (int i = 0; i < 3000000; i++) {
            String s = "item-" + i;
            String t = s + "!";
            t += s;
            total += s.length() + t.length();
        }
        for (int i = 0; i < 300000; i++) {
            int[] xs = new int[16];
            int[] ys = {i, i + 1, i + 2};
            xs[3] = i;
            total += xs[3] + xs.length + ys[2];
        }
        for (int i = 0; i < 100000; i++) {
            int[][] grid = new int[8][16];
            grid[3][4] = i;
            total += grid[3][4] + grid.length + grid[0].length;
        }
        System.out.println(total);
    }
}
JAVA
done
peak() { python3 - "$@" <<'PY'
import resource, subprocess, sys
r = subprocess.run(sys.argv[1:], capture_output=True, text=True)
print(resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss // 1024, r.stdout.strip().splitlines()[-1] if r.stdout.strip() else r.stderr[:200])
PY
}
on=$(peak ../../bin/ashvm run "$TMP/on/Churn.ash")
off=$(peak ../../bin/ashvm run "$TMP/off/Churn.ash")
on_mb=${on%% *}; off_mb=${off%% *}
echo "memory-safety on: ${on}  off: ${off}  (peak MB, last line of output)"
[ "${on#* }" == "${off#* }" ] || { echo "FAIL: outputs differ"; exit 1; }
[ "$on_mb" -le 64 ] || { echo "FAIL: on-mode peak ${on_mb} MB is not bounded"; exit 1; }
[ "$off_mb" -ge $((on_mb * 4)) ] || { echo "FAIL: off-mode peak ${off_mb} MB is not clearly larger"; exit 1; }
echo "memory check ok"
