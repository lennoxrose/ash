#!/usr/bin/env bash
# apm/tests/scripts/hub_test.sh -- the hub side of apm, offline: a fake hub (fake_hub.py) answers the API, and the
# "GitHub" it points at is a directory of bare git repos (APM_GIT_BASE). Covers login (token and browser flow), the
# saved credentials, adding packages by owner/repo (pinned commits, transitive dependencies), the CI commands, and
# hostile answers from a server. smoke_test.sh remains the check against the real GitHub.
set -euo pipefail
cd "$(dirname "$0")/../.." # -> apm/

make -s

BIN="$(cd ../../bin && pwd)/apm"
SCRATCH=$(mktemp -d)
HUB_PID=""
cleanup() { [ -n "$HUB_PID" ] && kill "$HUB_PID" 2>/dev/null || true; rm -rf "$SCRATCH"; }
trap cleanup EXIT
fail() { echo "FAIL: $1"; exit 1; }
TOKEN="apm:$(printf 'A%.0s' $(seq 86))"

# --- two packages on a "GitHub": alice/base v1.0.0 and alice/greeter v1.0.0, v1.1.0 (depends on base)
mkpkg() { # <owner> <repo> <dependencies>
    local w="$SCRATCH/work/$1/$2"
    mkdir -p "$w/src/main/ash/$2" "$SCRATCH/git/$1"
    git -C "$SCRATCH/work/$1" init -q "$2"
    printf 'name: %s\nversion: 1.0.0\n%s' "$2" "$3" > "$w/ash.pkg"
    echo "package $2;" > "$w/src/main/ash/$2/Main.ash"
}
commit() { git -C "$SCRATCH/work/$1/$2" add -A; git -C "$SCRATCH/work/$1/$2" -c user.name=t -c user.email=t@t commit -q -m "$3"; git -C "$SCRATCH/work/$1/$2" tag "$4"; }
mkpkg alice base ""
commit alice base v1 v1.0.0
mkpkg alice greeter $'\n[dependencies]\nbase: alice/base@v1.0.0\n'
commit alice greeter v1 v1.0.0
echo "// 1.1" >> "$SCRATCH/work/alice/greeter/src/main/ash/greeter/Main.ash"
commit alice greeter v2 v1.1.0
for r in base greeter; do
    git clone -q --bare "$SCRATCH/work/alice/$r" "$SCRATCH/git/alice/$r.git"
    git -C "$SCRATCH/git/alice/$r.git" config uploadpack.allowAnySHA1InWant true
done
export APM_GIT_BASE="file://$SCRATCH/git"

python3 -u tests/scripts/fake_hub.py "$SCRATCH/git" "$SCRATCH/hub.log" > "$SCRATCH/port" &
HUB_PID=$!
for _ in $(seq 50); do [ -s "$SCRATCH/port" ] && break; sleep 0.1; done
export APM_HUB="http://127.0.0.1:$(head -1 "$SCRATCH/port")"
export APM_CONFIG_DIR="$SCRATCH/cfg"
unset APM_TOKEN
touch "$SCRATCH/hub.log"
cd "$SCRATCH"; mkdir proj; cd proj

# --- logged out
"$BIN" auth status > /dev/null 2>&1 && fail "status while logged out should exit non-zero"
if "$BIN" link alice/greeter > out.txt 2>&1; then fail "link without a login must fail"; fi
grep -q "not logged in" out.txt || fail "link without a login should say so"

# --- login with a token: validated, saved 0600, shown by status, removed by logout
if echo "apm:short" | "$BIN" auth login --with-token > /dev/null 2>&1; then fail "a malformed token must be refused"; fi
if echo "apm:$(printf 'B%.0s' $(seq 86))" | "$BIN" auth login --with-token > /dev/null 2>&1; then fail "a token the hub rejects must be refused"; fi
[ ! -e "$APM_CONFIG_DIR/hosts" ] || fail "a refused token must not be saved"
echo "$TOKEN" | "$BIN" auth login --with-token > /dev/null || fail "login --with-token"
[ "$(stat -c %a "$APM_CONFIG_DIR/hosts")" = "600" ] || fail "hosts file must be 0600"
[ "$(stat -c %a "$APM_CONFIG_DIR")" = "700" ] || fail "config dir must be 0700"
"$BIN" auth status | grep "logged in to $APM_HUB as alice" >/dev/null || fail "status"
[ "$("$BIN" auth token)" = "$TOKEN" ] || fail "auth token"
"$BIN" auth logout > /dev/null || fail "logout"
[ ! -e "$APM_CONFIG_DIR/hosts" ] || fail "logout must remove the saved login"
if "$BIN" auth token > /dev/null 2>&1; then fail "auth token after logout should fail"; fi

# --- login with the browser flow (device code): pending, slow_down, then the token
"$BIN" auth login --web --no-browser --name "ci box" > web.txt || fail "login --web"
grep -q "BCDF-GHJK" web.txt || fail "the one-time code must be shown"
grep -q "/login/device" web.txt || fail "the verification URL must be shown"
grep -q "^token: $TOKEN" "$APM_CONFIG_DIR/hosts" || fail "login --web must save the token"
grep -q "ci box" "$SCRATCH/hub.log" || true
# a saved token is never sent to a different hub than the one it came from
APM_HUB="http://127.0.0.1:1" "$BIN" auth status > /dev/null 2>&1 && fail "status against another hub should not use the saved token"
: > "$SCRATCH/hub.log"; APM_HUB="http://127.0.0.1:1" "$BIN" link alice/greeter > out.txt 2>&1 || true
grep -q "not logged in" out.txt || fail "the saved token must not be used for another hub"
# APM_TOKEN wins over the file and is not written anywhere
rm -rf "$APM_CONFIG_DIR"; APM_TOKEN="$TOKEN" "$BIN" auth status | grep "APM_TOKEN" >/dev/null || fail "APM_TOKEN"
[ ! -e "$APM_CONFIG_DIR/hosts" ] || fail "APM_TOKEN must not create a file"

# --- adding by owner/repo: newest tag pinned, exact commit installed, dependencies of dependencies too
"$BIN" add alice/greeter > /dev/null || fail "add alice/greeter"
grep -q "^greeter: alice/greeter@v1.1.0$" ash.pkg || fail "newest tag must be recorded"
[ -d @ash-modules/base ] || fail "transitive dependency not installed"
grep -q "1.1" @ash-modules/greeter/src/main/ash/greeter/Main.ash || fail "v1.1.0 content"
want=$(git -C "$SCRATCH/git/alice/greeter.git" rev-parse 'v1.1.0^{commit}')
grep -q "^greeter: alice/greeter@v1.1.0 $want$" ash.lock || fail "ash.lock must hold the resolved commit"
[ "$(git -C @ash-modules/greeter rev-parse HEAD)" = "$want" ] || fail "checked out commit"
"$BIN" list | grep "greeter: alice/greeter@v1.1.0 (${want:0:12})" >/dev/null || fail "list shows the commit"
grep -q "resolve" "$SCRATCH/hub.log" && ! grep "resolve" "$SCRATCH/hub.log" | grep "auth=yes" >/dev/null || fail "resolving must be anonymous"
rm -rf @ash-modules; "$BIN" install > /dev/null || fail "install from ash.pkg"
[ -d @ash-modules/base ] || fail "install must restore transitive dependencies"
[ "$(git -C @ash-modules/greeter rev-parse HEAD)" = "$want" ] || fail "install must restore the locked commit"
"$BIN" install --locked > /dev/null || fail "install --locked with hub packages"
"$BIN" add alice/greeter@v1.0.0 --as old-greeter > /dev/null || fail "add with a version"
! grep -q "1.1" @ash-modules/old-greeter/src/main/ash/greeter/Main.ash || fail "v1.0.0 content"
"$BIN" add github:alice/base --dev --as base-dev > /dev/null || fail "github: spec still works"
grep -q "^base-dev: github:alice/base$" ash.pkg || fail "github: dependency recorded"

# --- failures change nothing
cp ash.pkg before.pkg
if "$BIN" add alice/nope > /dev/null 2>&1; then fail "unknown package must fail"; fi
if "$BIN" add alice/greeter@v9.9.9 --as nine > /dev/null 2>&1; then fail "unknown version must fail"; fi
cmp -s ash.pkg before.pkg || fail "a failed add must not change ash.pkg"
for bad in 'alice/../x' 'a/b/c' 'ali ce/x' 'alice/greeter;touch pwned' '/x' 'alice/'; do
    if "$BIN" add "$bad" > /dev/null 2>&1; then fail "'$bad' must be refused"; fi
done
if "$BIN" add alice/greeter --as ../evil > /dev/null 2>&1; then fail "--as ../evil must be refused"; fi
# a hostile hub: shell syntax in the repository, a nesting bomb, a 2 MB body, a foreign host
for h in shell deep big host; do
    if "$BIN" add "evil/$h" --as "evil-$h" > /dev/null 2>&1; then fail "hostile hub answer '$h' must be refused"; fi
done
[ ! -e pwned ] || fail "shell syntax from the hub was executed"

# --- the CI commands
echo "$TOKEN" | "$BIN" auth login --with-token > /dev/null
"$BIN" link alice/greeter | grep "linked on the hub" >/dev/null || fail "link"
"$BIN" link alice/greeter | grep "already linked" >/dev/null || fail "link is idempotent"
"$BIN" packages | grep "alice/greeter" >/dev/null || fail "packages"
"$BIN" sync alice/greeter | grep "synced" >/dev/null || fail "sync"
printf 'name: x\nrepository: github:alice/greeter@main\n' > ash.pkg
"$BIN" sync | grep "alice/greeter" >/dev/null || fail "sync defaults to repository: of ash.pkg"
"$BIN" unlink alice/greeter | grep "unlinked" >/dev/null || fail "unlink"
"$BIN" packages | grep "alice/greeter" >/dev/null && fail "unlinked package still listed"
"$BIN" search greeter | grep "alice/greeter" >/dev/null || fail "search"
"$BIN" info alice/greeter | grep "apm add alice/greeter" >/dev/null || fail "info"
grep -q "PUT /api/v1/me/packages/alice/greeter auth=yes" "$SCRATCH/hub.log" || fail "link must send the token"
grep "search" "$SCRATCH/hub.log" | grep "auth=yes" >/dev/null && fail "search must not send the token"

echo "apm hub tests passed"
