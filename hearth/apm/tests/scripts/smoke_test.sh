#!/usr/bin/env bash
# apm/tests/scripts/smoke_test.sh -- exercises the CLI end to end
# (init/add/install/list/run) against a throwaway scratch directory. Not a
# .ash regression suite like ashvm/kiln have (apm doesn't run Ash
# programs, it manages their dependencies) -- this is the equivalent
# check for this project: does the real CLI, talking to the real
# filesystem and the real `git`, do what it says.
set -euo pipefail
cd "$(dirname "$0")/../.." # -> apm/
APM_DIR="$PWD"

make -s

BIN="$(cd ../../bin && pwd)/apm" # absolute -- the script cd's into a scratch dir below
SCRATCH=$(mktemp -d)
trap 'rm -rf "$SCRATCH"' EXIT

fail() { echo "FAIL: $1"; exit 1; }

cd "$SCRATCH"

"$BIN" init > /dev/null || fail "init"
[ -f ash.pkg ] || fail "ash.pkg not created"
grep -q "^name:" ash.pkg || fail "ash.pkg missing name"

"$BIN" add github:lennoxrose/ash-stdlib > /dev/null || fail "add"
grep -q "ash-stdlib: github:lennoxrose/ash-stdlib" ash.pkg || fail "dependency not recorded"
[ -d @ash-modules/ash-stdlib ] || fail "dependency not installed"

rm -rf @ash-modules
"$BIN" install > /dev/null || fail "install"
[ -d @ash-modules/ash-stdlib ] || fail "install did not restore the dependency"

"$BIN" install > /dev/null || fail "re-install (idempotency)"

"$BIN" list | grep -q "ash-stdlib" || fail "list didn't show the dependency"

# full manifest: every field and section survives a load/save round trip (add rewrites ash.pkg)
cat > ash.pkg <<'PKG'
name: demo
version: 1.2.0
description: A demo package
author: Ada <ada@example.com>
license: MIT
homepage: https://example.com/demo
repository: github:ada/demo
bugs: https://example.com/demo/issues
main: demo.Main
keywords: ash, demo

[dependencies]
ash-stdlib: github:lennoxrose/ash-stdlib

[libraries]
m: system
vulkan: pkg-config:vulkan

[scripts]
build: echo building > built.txt
test: echo testing
PKG
"$BIN" add github:lennoxrose/ash-stdlib --as stdlib-dev --dev > /dev/null || fail "add --dev"
for want in "^description: A demo package" "^author: Ada <ada@example.com>" "^license: MIT" "^main: demo.Main" \
            "^keywords: ash, demo" "^m: system" "^vulkan: pkg-config:vulkan" "^build: echo building" \
            "^\[dev-dependencies\]" "^stdlib-dev: github:lennoxrose/ash-stdlib"; do
    grep -q -- "$want" ash.pkg || fail "ash.pkg lost '$want' after add --dev"
done
[ -d @ash-modules/stdlib-dev ] || fail "dev dependency not installed"
"$BIN" list | grep -q "vulkan: pkg-config:vulkan" || fail "list didn't show libraries"

"$BIN" build > /dev/null || fail "apm build"
[ "$(cat built.txt)" == "building" ] || fail "build script did not run"
out=$("$BIN" run test) && grep -q "^testing$" <<<"$out" || fail "apm run test"
if "$BIN" run nonexistent > /dev/null 2>&1; then fail "unknown script should exit non-zero"; fi

# licenses: write LICENSE and keep ash.pkg in sync; running again replaces both
"$BIN" license MIT --year 2025 > /dev/null || fail "apm license MIT"
grep -q "^Copyright (c) 2025 Ada <ada@example.com>$" LICENSE && fail "email must not end up in the copyright line"
grep -q "^Copyright (c) 2025 Ada$" LICENSE || fail "LICENSE copyright line (author from ash.pkg)"
grep -q "^license: MIT$" ash.pkg || fail "ash.pkg license not set to MIT"
"$BIN" license gpl-3.0-only > /dev/null || fail "apm license GPL-3.0 (alias, replace)"
grep -q "GNU GENERAL PUBLIC LICENSE" LICENSE || fail "LICENSE not replaced"
grep -q "^license: GPL-3.0$" ash.pkg || fail "ash.pkg license not updated"
grep -q "^build: echo building" ash.pkg || fail "ash.pkg lost other fields on license change"
"$BIN" license | grep -q "OFL-1.1" || fail "license list"
if "$BIN" license no-such-license > /dev/null 2>&1; then fail "unknown license should exit non-zero"; fi

# new + check: scaffold follows the conventions once a license is chosen
"$BIN" new demo-mod > /dev/null || fail "apm new"
[ -f demo-mod/src/main/ash/demo_mod/DemoMod.ash ] || fail "apm new source file"
(cd demo-mod && if "$BIN" check > /dev/null 2>&1; then fail "check should flag the missing license"; fi
 "$BIN" license MIT --author t > /dev/null && "$BIN" check > /dev/null) || fail "apm check on scaffold"
if "$BIN" new demo-mod > /dev/null 2>&1; then fail "apm new over existing dir should fail"; fi

# ash.pkg is edited, not regenerated: comments, blank lines, unknown keys and sections and the author's order survive
mkdir keep && (cd keep
printf '# my package\nname: keep\nversion: 1.0.0\nmemory_safety: off\n\n# scripts\n[scripts]\n# build it\nbuild: echo hi\n\n[weird]\nk: v\n' > ash.pkg
"$BIN" license MIT --author me > /dev/null 2>&1 || fail "license on a hand-written ash.pkg"
for want in "^# my package$" "^memory_safety: off$" "^# scripts$" "^# build it$" "^\[weird\]$" "^k: v$" "^license: MIT$"; do
    grep -q -- "$want" ash.pkg || fail "ash.pkg lost '$want' when apm rewrote it"
done
[ "$(grep -n '^license: MIT$' ash.pkg | cut -d: -f1)" -lt "$(grep -n '^\[scripts\]$' ash.pkg | cut -d: -f1)" ] || fail "new header key must land in the header")
# a script longer than 255 characters (the old value limit) is kept whole and runs whole
long=$(printf 'x%.0s' $(seq 600))
mkdir longline && (cd longline
printf 'name: l\nversion: 1\n\n[scripts]\nbig: echo %s > out.txt\n' "$long" > ash.pkg
"$BIN" run big > /dev/null || fail "apm run of a long script"
[ "$(wc -c < out.txt)" -eq 601 ] || fail "long script was cut"
"$BIN" license MIT --author me > /dev/null 2>&1
grep -q "^big: echo $long > out.txt$" ash.pkg || fail "long script cut by a rewrite")

# a dependency name is a directory and ends up on a git command line: it must never reach a shell
mkdir inject && (cd inject
printf 'name: i\nversion: 1\n\n[dependencies]\nx; touch pwned: github:lennoxrose/ash-stdlib\n../escape: github:lennoxrose/ash-stdlib\n' > ash.pkg
if "$BIN" install > /dev/null 2>&1; then fail "install must reject unusable dependency names"; fi
[ ! -e pwned ] || fail "a dependency name was run by a shell"
[ ! -e ../escape ] || fail "a dependency name escaped @ash-modules"
if "$BIN" add github:lennoxrose/ash-stdlib --as '../x' > /dev/null 2>&1; then fail "add --as ../x must be rejected"; fi
if "$BIN" add github:lennoxrose/ash-stdlib --bogus > /dev/null 2>&1; then fail "add with an unknown option must fail"; fi
if "$BIN" add 'github:lennoxrose/ash-stdlib;touch' > /dev/null 2>&1; then fail "add with a bad spec must fail"; fi
[ ! -e touch ] || fail "a source spec was run by a shell")

# a dependency that cannot be fetched is not recorded
mkdir norepo && (cd norepo
printf 'name: n\nversion: 1\n' > ash.pkg
cp ash.pkg before.pkg
if "$BIN" add github:lennoxrose/ash-no-such-repo-0xdeadbeef > /dev/null 2>&1; then fail "add of a missing repository must fail"; fi
cmp -s ash.pkg before.pkg || fail "failed add changed ash.pkg")

# apm check: unknown keys, memory-safety settings, dependency specs, main
"$BIN" new chk > /dev/null && (cd chk
"$BIN" license MIT --author t > /dev/null
"$BIN" check > /dev/null 2>&1 || fail "check on a clean scaffold"
grep -q "^memory-safety: on$" ash.pkg || fail "scaffold should set memory-safety: on"
sed -i 's/^memory-safety: on$/memory_safety: on/' ash.pkg
out=$("$BIN" check 2>&1) || fail "an unknown key is a warning, not a failure"
grep -q "unknown key 'memory_safety'.*did you mean 'memory-safety'" <<<"$out" || fail "check: unknown key not reported ($out)"
sed -i 's/^memory_safety: on$/memory-safety: false/' ash.pkg
if out=$("$BIN" check 2>&1); then fail "memory-safety: false must fail the check"; fi
grep -q "memory-safety must be 'on' or 'off'" <<<"$out" || fail "check: bad memory-safety value not reported ($out)"
sed -i 's/^memory-safety: false$/memory-safety: off/' ash.pkg
out=$("$BIN" check 2>&1) || fail "memory-safety: off is a warning"
grep -q "memory-safety is off" <<<"$out" || fail "check: memory-safety off not mentioned ($out)"
sed -i '/^memory-safety: off$/d' ash.pkg
printf '\n[scripts]\nmemory-safety: off\n' >> ash.pkg
out=$("$BIN" check 2>&1) || fail "memory-safety under a section is a warning"
grep -q "only reads it in the header" <<<"$out" || fail "check: misplaced memory-safety not reported ($out)"
sed -i '/^memory-safety: off$/d' ash.pkg
printf '\n[dependencies]\nlib: path:../lib\nbad;name: github:u/r\n' >> ash.pkg
if out=$("$BIN" check 2>&1); then fail "unusable dependencies must fail the check"; fi
grep -q "'path:../lib' is not a source apm can fetch" <<<"$out" || fail "check: bad source not reported ($out)"
grep -q "'bad;name' is not a usable name" <<<"$out" || fail "check: bad name not reported ($out)"
sed -i '/^\[dependencies\]$/,$d' ash.pkg
sed -i 's/^main: .*/main: nope.Missing/' ash.pkg
if out=$("$BIN" check 2>&1); then fail "a main that does not exist must fail the check"; fi
grep -q "main 'nope.Missing' is not a file" <<<"$out" || fail "check: missing main not reported ($out)")

# fsutil_rm_rf (the clean-up of a module without src/main/ash) removes a tree whose paths are longer than any
# fixed buffer: it used to cut such a path short and fail to empty the directory
(mkdir deep_rm && cd deep_rm
cat > t.c <<'C'
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "H/fsutil.h"
int main(void) {
    char path[4000] = "tree";
    mkdir(path, 0755);
    char comp[201];
    memset(comp, 'a', 200);
    comp[200] = 0;
    for (int i = 0; i < 9; i++) {                    /* 9 levels of 200 characters: 1800 deep */
        strcat(path, "/");
        strcat(path, comp);
        if (mkdir(path, 0755) != 0) { perror("mkdir"); return 2; }
    }
    FILE *f = fopen("tree/top.txt", "w"); if (f) fclose(f);
    f = fopen(strcat(path, "/leaf.txt"), "w"); if (f) fclose(f);
    if (!fsutil_rm_rf("tree")) return 3;
    struct stat st;
    return stat("tree", &st) == 0 ? 4 : 0;
}
C
gcc -std=c11 -Wall -Wextra -I "$APM_DIR/src" -o t t.c "$APM_DIR/src/C/fsutil.c" 2>/dev/null || fail "could not build the rm_rf test"
./t || fail "fsutil_rm_rf left part of a deeply nested tree behind (exit $?)")

# ash.lock: commits are pinned, install honours them, update moves them, remove prunes them, transitive dependencies
# come from the dependency's own ash.pkg. No network: git is pointed at repositories made here, which stand in for
# https://github.com/acme/<name>.git (git's url.<base>.insteadOf).
GITROOT="$SCRATCH/gitroot"
mkdir -p "$GITROOT/acme"
cat > "$SCRATCH/gitconfig" <<CFG
[url "file://$GITROOT/"]
	insteadOf = https://github.com/
[user]
	name = apm test
	email = apm@test.invalid
[init]
	defaultBranch = main
CFG
export GIT_CONFIG_GLOBAL="$SCRATCH/gitconfig" GIT_CONFIG_NOSYSTEM=1
upstream() { echo "$GITROOT/acme/$1.git"; }
# mkpkg <name> [dependency lines of its ash.pkg]: a module with one commit
mkpkg() {
    local d; d=$(upstream "$1")
    mkdir -p "$d/src/main/ash/$1"
    git -C "$d" init -q
    printf 'package %s;\npublic class C { public static String v() { return "%s"; } }\n' "$1" "$1" > "$d/src/main/ash/$1/C.ash"
    { printf 'name: %s\nversion: 1.0.0\n' "$1"; if [ $# -gt 1 ]; then printf '\n[dependencies]\n'; shift; printf '%s\n' "$@"; fi; } > "$d/ash.pkg"
    git -C "$d" add -A
    git -C "$d" commit -q -m "$1 one"
}
# more <name> <text>: another commit on the default branch
more() { echo "$2" >> "$(upstream "$1")/extra.txt"; git -C "$(upstream "$1")" add -A; git -C "$(upstream "$1")" commit -q -m "$2"; }
tip() { git -C "$(upstream "$1")" rev-parse "${2:-HEAD}"; }
at() { git -C "$1" rev-parse HEAD; }

mkpkg libb
git -C "$(upstream libb)" tag v1
mkpkg liba 'libb: github:acme/libb@v1'
mkpkg tracker
mkdir lockproj && (cd lockproj
"$BIN" init > /dev/null || fail "lock: init"
liba1=$(tip liba); libb1=$(tip libb v1); tracker1=$(tip tracker)

# add installs the dependency and what its ash.pkg needs, and writes ash.lock
"$BIN" add github:acme/liba > /dev/null 2>&1 || fail "lock: add of a package with a dependency"
[ -f ash.lock ] || fail "lock: add did not write ash.lock"
grep -q "^liba: github:acme/liba $liba1\$" ash.lock || fail "lock: liba not pinned to its commit ($(cat ash.lock))"
grep -q "^libb: github:acme/libb@v1 $libb1 via liba\$" ash.lock || fail "lock: transitive libb not pinned / not marked via liba"
[ -f @ash-modules/libb/src/main/ash/libb/C.ash ] || fail "lock: transitive dependency not installed next to the others"
grep -q "libb" ash.pkg && fail "lock: a transitive dependency must not be written to ash.pkg"
"$BIN" list | grep -q "libb: github:acme/libb@v1 (${libb1:0:12}, via liba)" || fail "lock: list does not show the transitive package"

# upstream moves on; a clean install still gives the locked commits (also for a tag that was moved)
more liba "liba two"; more libb "libb two"; git -C "$(upstream libb)" tag -f v1 > /dev/null
cp ash.lock lock.before
rm -rf @ash-modules
"$BIN" install > /dev/null 2>&1 || fail "lock: install from ash.lock"
[ "$(at @ash-modules/liba)" == "$liba1" ] || fail "lock: install did not use the locked commit of liba"
[ "$(at @ash-modules/libb)" == "$libb1" ] || fail "lock: install did not use the locked commit of libb (the tag has moved)"
[ ! -e @ash-modules/liba/extra.txt ] || fail "lock: install fetched newer files than the lock"
cmp -s ash.lock lock.before || fail "lock: install changed ash.lock"
"$BIN" install > /dev/null 2>&1 || fail "lock: second install"
cmp -s ash.lock lock.before || fail "lock: a second install changed ash.lock"
"$BIN" install --locked > /dev/null 2>&1 || fail "lock: install --locked on a current lockfile"

# a package that is somewhere else than the lock says is put back
git -C @ash-modules/liba commit -q --allow-empty -m drift
[ "$(at @ash-modules/liba)" != "$liba1" ] || fail "lock: test setup (drift)"
"$BIN" install > /dev/null 2>&1 || fail "lock: install over a drifted checkout"
[ "$(at @ash-modules/liba)" == "$liba1" ] || fail "lock: install did not restore the locked commit"

# a dev dependency is locked too, and a branch is pinned to the commit it had
"$BIN" add github:acme/tracker --dev > /dev/null 2>&1 || fail "lock: add --dev"
grep -q "^tracker: github:acme/tracker $tracker1\$" ash.lock || fail "lock: dev dependency not pinned"
more tracker "tracker two"
rm -rf @ash-modules/tracker
"$BIN" install > /dev/null 2>&1 || fail "lock: reinstall of a branch dependency"
[ "$(at @ash-modules/tracker)" == "$tracker1" ] || fail "lock: a branch dependency moved without an update"

# --locked refuses to change anything: a package ash.lock does not know
cp ash.lock lock.full
grep -v '^tracker:' lock.full > ash.lock
if "$BIN" install --locked > /dev/null 2>&1; then fail "lock: --locked accepted a lockfile without tracker"; fi
cmp -s ash.lock <(grep -v '^tracker:' lock.full) || fail "lock: --locked rewrote ash.lock"
cp lock.full ash.lock

# update <name> moves that package (and what only it brought in), nothing else
"$BIN" update tracker > /dev/null 2>&1 || fail "lock: update tracker"
grep -q "^tracker: github:acme/tracker $(tip tracker)\$" ash.lock || fail "lock: update did not move tracker"
[ "$(at @ash-modules/tracker)" == "$(tip tracker)" ] || fail "lock: update did not check out the new tracker commit"
grep -q "^liba: github:acme/liba $liba1\$" ash.lock || fail "lock: update tracker moved liba"
"$BIN" update liba > /dev/null 2>&1 || fail "lock: update liba"
grep -q "^liba: github:acme/liba $(tip liba)\$" ash.lock || fail "lock: update liba did not move liba"
grep -q "^libb: github:acme/libb@v1 $(tip libb v1) via liba\$" ash.lock || fail "lock: update liba did not look libb up again (its tag moved)"
[ -f @ash-modules/liba/extra.txt ] || fail "lock: update liba did not fetch the new files"
more tracker "tracker three"; more liba "liba three"
"$BIN" update > /dev/null 2>&1 || fail "lock: update (all)"
grep -q "^tracker: github:acme/tracker $(tip tracker)\$" ash.lock || fail "lock: update (all) did not move tracker"
grep -q "^liba: github:acme/liba $(tip liba)\$" ash.lock || fail "lock: update (all) did not move liba"
if "$BIN" update nosuchpackage > /dev/null 2>&1; then fail "lock: update of an unknown package must fail"; fi

# remove: out of ash.pkg, out of @ash-modules together with what only it needed, out of ash.lock
if "$BIN" remove libb > /dev/null 2>&1; then fail "lock: remove of a transitive package must fail"; fi
if "$BIN" remove nosuchpackage > /dev/null 2>&1; then fail "lock: remove of an unknown package must fail"; fi
"$BIN" remove liba > /dev/null 2>&1 || fail "lock: remove liba"
grep -q "liba" ash.pkg && fail "lock: remove left liba in ash.pkg"
[ ! -e @ash-modules/liba ] || fail "lock: remove left liba installed"
[ ! -e @ash-modules/libb ] || fail "lock: remove left the package only liba needed"
grep -q "liba\|libb" ash.lock && fail "lock: remove left packages in ash.lock"
[ -d @ash-modules/tracker ] || fail "lock: remove took tracker too"
grep -q "^tracker: " ash.lock || fail "lock: remove dropped tracker from ash.lock"
"$BIN" remove tracker > /dev/null 2>&1 || fail "lock: remove tracker"
[ ! -e ash.lock ] || fail "lock: ash.lock must go when no package is left"
)

# the first source asked for a name stays (and the other is a warning); a package needed from two places is installed once
mkpkg other 'libb: github:acme/libb'
mkdir lockconflict && (cd lockconflict
printf 'name: c\nversion: 1\n\n[dependencies]\nlibb: github:acme/libb@v1\nother: github:acme/other\n' > ash.pkg
out=$("$BIN" install 2>&1) || fail "lock: install with a source conflict ($out)"
grep -q "keeping that one" <<<"$out" || fail "lock: a conflicting source was not reported ($out)"
grep -q "^libb: github:acme/libb@v1 $(tip libb v1)\$" ash.lock || fail "lock: the first source of libb did not win"
grep -q "^other: github:acme/other .* via" ash.lock && fail "lock: other is a dependency of the project"
"$BIN" install --locked > /dev/null 2>&1 || fail "lock: --locked after a conflict")

# a dependency's own ash.pkg is not trusted: bad names and sources are refused, nothing is written
mkpkg evil '../escape: github:acme/libb'
mkpkg evil2 'libb: github:acme/libb;touch pwned'
for bad in evil evil2; do
    mkdir "lock_$bad" && (cd "lock_$bad"
    printf 'name: e\nversion: 1\n\n[dependencies]\n%s: github:acme/%s\n' "$bad" "$bad" > ash.pkg
    if "$BIN" install > /dev/null 2>&1; then fail "lock: install accepted a dependency with a bad dependency ($bad)"; fi
    [ ! -e ash.lock ] || fail "lock: a failed install wrote ash.lock ($bad)"
    [ ! -e ../escape ] && [ ! -e pwned ] && [ ! -e @ash-modules/escape ] || fail "lock: a bad dependency escaped ($bad)")
done

# a lockfile naming a commit that does not exist fails the install and leaves the lockfile alone
mkdir lockbad && (cd lockbad
printf 'name: b\nversion: 1\n\n[dependencies]\nlibb: github:acme/libb@v1\n' > ash.pkg
"$BIN" install > /dev/null 2>&1 || fail "lock: install for the bad-lock test"
sed -i "s/ $(tip libb v1)\$/ 0123456789abcdef0123456789abcdef01234567/" ash.lock
cp ash.lock lock.bad
rm -rf @ash-modules
if "$BIN" install > /dev/null 2>&1; then fail "lock: install accepted a commit that does not exist"; fi
cmp -s ash.lock lock.bad || fail "lock: a failed install changed ash.lock"
printf 'rubbish\n' > ash.lock
"$BIN" install > /dev/null 2>&1 || fail "lock: an unreadable lockfile must be replaced, not fail the install"
grep -q "^libb: github:acme/libb@v1 $(tip libb v1)\$" ash.lock || fail "lock: unreadable lockfile not rewritten")

if "$BIN" bogus-command > /dev/null 2>&1; then fail "unknown command should exit non-zero"; fi

echo "apm smoke test: OK"
