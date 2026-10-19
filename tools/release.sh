#!/bin/sh
# Cut an Ash release: build, test, package, smoke test, then tag and publish on GitHub.
#
#   tools/release.sh <tag> [--dry-run] [--yes]
#
#   <tag>      beta-vX.Y.Z or vX.Y.Z; must not exist locally or on origin
#   --dry-run  do everything up to and including the summary, then stop: no tag, no push, no release.
#              (a dirty tree / unpushed commits are reported but do not stop a dry run)
#   --yes      do not ask before tagging and publishing
#
# Needs: git, gh (authenticated), make, gcc, cargo (mnemo), GNU tar, sha256sum, curl, bwrap (smoke test).
# See docs/RELEASING.md. POSIX sh.
set -eu

ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"

# Platforms this script can build, test AND publish. It builds for the machine it runs on.
SUPPORTED="linux-x86_64"

die()  { printf 'release: %s\n' "$*" >&2; exit 1; }
say()  { printf '\n== %s\n' "$*"; }
DRY=0; YES=0; TAG=""
for a in "$@"; do
    case $a in
        --dry-run) DRY=1 ;;
        --yes) YES=1 ;;
        -h|--help) sed -n '2,13p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        -*) die "unknown option $a" ;;
        *) [ -z "$TAG" ] || die "only one tag allowed"; TAG=$a ;;
    esac
done
[ -n "$TAG" ] || die "usage: tools/release.sh <tag> [--dry-run] [--yes]"

# 1. tag format, tag must be new ------------------------------------------------------------------
say "tag $TAG"
printf '%s\n' "$TAG" | grep -Eq '^(beta-)?v[0-9]+\.[0-9]+\.[0-9]+$' \
    || die "tag must look like beta-v0.1.0 or v0.1.0"
git rev-parse -q --verify "refs/tags/$TAG" >/dev/null && die "tag $TAG already exists locally; releases never reuse a tag"
git remote get-url origin >/dev/null 2>&1 || die "no origin remote"
git fetch -q origin --tags
git rev-parse -q --verify "refs/tags/$TAG" >/dev/null && die "tag $TAG already exists on origin; releases never reuse a tag"
REPO=$(git remote get-url origin | sed -E 's#^(git@github.com:|https://github.com/)##; s#\.git$##')
printf 'repo %s\n' "$REPO"

# 2. git state and gh ------------------------------------------------------------------------------
say "git state"
problems=""
[ "$(git rev-parse --abbrev-ref HEAD)" = main ] || problems="$problems\n  - not on main"
[ -z "$(git status --porcelain)" ] || problems="$problems\n  - working tree is not clean"
[ "$(git rev-parse HEAD)" = "$(git rev-parse origin/main)" ] || problems="$problems\n  - HEAD is not origin/main (push or pull first)"
if [ -n "$problems" ]; then
    if [ "$DRY" = 1 ]; then printf 'dry run: a real release would stop here:%b\n' "$problems"
    else die "refusing to release:$(printf '%b' "$problems")"; fi
else
    echo "on main, clean, equal to origin/main"
fi
command -v gh >/dev/null || die "gh is not installed"
gh auth status >/dev/null 2>&1 || die "gh is not authenticated (gh auth login)"

# platform of this machine
OS=$(uname -s | tr 'A-Z' 'a-z')
case $(uname -m) in x86_64|amd64) ARCH=x86_64 ;; aarch64|arm64) ARCH=aarch64 ;; *) ARCH=$(uname -m) ;; esac
PLAT=$OS-$ARCH
case " $SUPPORTED " in *" $PLAT "*) ;; *) die "$PLAT is not a supported release platform (supported: $SUPPORTED); see docs/RELEASING.md" ;; esac
command -v bwrap >/dev/null || die "bwrap is needed for the smoke test sandbox"

# 8 (early, it is cheap). release notes from CHANGELOG.md ------------------------------------------
say "release notes"
WORK=$(mktemp -d); trap 'rm -rf "$WORK"' EXIT
NOTES=$WORK/notes.md
awk -v tag="$TAG" '
    /^## / { if (on) exit; h = $0; i = index(h, tag); if (i) { c = substr(h, i + length(tag), 1); p = i > 1 ? substr(h, i - 1, 1) : " "; if (c !~ /[0-9.]/ && p !~ /[A-Za-z0-9-]/) on = 1 } next }
    on { print }
' CHANGELOG.md > "$NOTES"
[ -s "$NOTES" ] && grep -q '[^[:space:]]' "$NOTES" \
    || die "CHANGELOG.md has no '## <date> ($TAG ...)' section for $TAG; write it first"
echo "$(wc -l < "$NOTES") lines of notes from CHANGELOG.md"

# 3. clean build with the version injected ---------------------------------------------------------
# Exported so every nested make (including the ones the test scripts start) links the same way.
say "clean build"
ASH_VERSION=$TAG; ASH_STATIC_RT=1; export ASH_VERSION ASH_STATIC_RT
make clean
make

# 4. tests ------------------------------------------------------------------------------------------
say "tests"
for t in hearth/mnemo/tests/scripts/run_regression.sh hearth/front/tests/scripts/check_types.sh \
         hearth/ashvm/tests/scripts/run_regression.sh hearth/kiln/tests/scripts/run_regression.sh \
         hearth/apm/tests/scripts/smoke_test.sh; do
    printf '%s ... ' "$t"
    if "$t" > "$WORK/test.log" 2>&1; then
        echo ok
    else
        tail -30 "$WORK/test.log"; die "test failed: $t"
    fi
done

# 5. package ------------------------------------------------------------------------------------------
say "package"
rm -rf dist; mkdir dist
STAGE=$WORK/stage; mkdir -p "$STAGE/bin"
for b in ashvm kiln apm; do install -m 755 "bin/$b" "$STAGE/bin/$b"; strip "$STAGE/bin/$b"; done
printf '%s\n' "$TAG" > "$STAGE/VERSION"
ASSET=ash-$PLAT.tar.gz
tar --sort=name --owner=0 --group=0 --numeric-owner --mtime=@0 -czf "dist/$ASSET" -C "$STAGE" bin VERSION
( cd dist && sha256sum ash-*.tar.gz > SHA256SUMS )
tar -tzf "dist/$ASSET"

# 6. smoke test: a sandbox with only the libraries the binaries list, no PATH, no home, no network ---
say "smoke test ($ASSET)"
HOME_ASH=$WORK/ash; mkdir -p "$HOME_ASH"; tar -xzf "dist/$ASSET" -C "$HOME_ASH"
[ "$(cat "$HOME_ASH/VERSION")" = "$TAG" ] || die "VERSION file does not say $TAG"
mkdir -p "$WORK/cwd"; cp examples/HelloWorld.ash "$WORK/cwd/"
LIBS=$(for b in ashvm kiln apm; do ldd "$HOME_ASH/bin/$b"; done | awk '/=>/ {print $3} !/=>/ && /^[ \t]*\// {print $1}' | sort -u)
echo "runtime libraries used by the binaries:"; for l in $LIBS; do echo "  $l"; done
BW="bwrap --unshare-all --die-with-parent --clearenv --ro-bind $HOME_ASH /ash --bind $WORK/cwd /work --chdir /work --proc /proc --dev /dev"
for l in $LIBS; do BW="$BW --ro-bind $(readlink -f "$l") $l"; done
INTERP=$(readelf -l "$HOME_ASH/bin/kiln" | sed -n 's/.*interpreter: \(.*\)\]/\1/p')
BW="$BW --ro-bind $(readlink -f "$INTERP") $INTERP"
sandbox() { $BW "$@"; }
for b in ashvm kiln apm; do
    got=$(sandbox /ash/bin/$b --version) || die "$b --version failed in the sandbox"
    [ "$got" = "$b $TAG" ] || die "$b --version printed '$got', expected '$b $TAG'"
    echo "$got"
done
got=$(sandbox /ash/bin/ashvm run HelloWorld.ash) || die "ashvm run failed in the sandbox"
[ "$got" = "Hello, Ash!" ] || die "ashvm run printed '$got'"
echo "ashvm run HelloWorld.ash -> $got"
sandbox /ash/bin/kiln HelloWorld.ash -o /work/hello || die "kiln failed in the sandbox"
got=$(sandbox /work/hello) || die "the kiln-compiled program failed"
[ "$got" = "Hello, Ash!" ] || die "kiln output printed '$got'"
echo "kiln HelloWorld.ash -> ./hello -> $got"
sandbox /ash/bin/apm new smoke >/dev/null || die "apm new failed in the sandbox"
[ -f "$WORK/cwd/smoke/ash.pkg" ] || die "apm new created no ash.pkg"
echo "apm new smoke -> ash.pkg created"

# 7. checksums ------------------------------------------------------------------------------------------
say "checksums"
( cd dist && sha256sum -c SHA256SUMS )

# 9. summary ---------------------------------------------------------------------------------------------
say "summary"
echo "tag      $TAG   (title: Ash $TAG, latest, not a pre-release)"
echo "commit   $(git rev-parse --short HEAD)"
echo "assets:"
for f in dist/*; do printf '  %-26s %8s bytes\n' "$(basename "$f")" "$(wc -c < "$f")"; done
echo "SHA256SUMS:"; sed 's/^/  /' dist/SHA256SUMS
echo "download https://github.com/$REPO/releases/latest/download/<asset>"
echo "notes ----"; cat "$NOTES"; echo "----"
if [ "$DRY" = 1 ]; then echo; echo "dry run: nothing was tagged, pushed or published."; exit 0; fi
if [ "$YES" != 1 ]; then
    printf '\nTag %s, push it and publish the release on %s? [y/N] ' "$TAG" "$REPO"
    read -r ans
    case $ans in y|Y|yes) ;; *) echo "aborted; dist/ is kept."; exit 1 ;; esac
fi

# 10. tag and publish ---------------------------------------------------------------------------------------
say "publish"
git tag -a "$TAG" -m "Ash $TAG"
git push origin "refs/tags/$TAG"
gh release create "$TAG" dist/* --repo "$REPO" --title "Ash $TAG" --notes-file "$NOTES" --latest --verify-tag \
    || die "the tag $TAG is pushed but the release was not created. Do not reuse the tag: fix it per docs/RELEASING.md"

# 11. verify what the public URL serves ------------------------------------------------------------------------
say "verify download"
V=$WORK/verify; mkdir -p "$V"
base=https://github.com/$REPO/releases/latest/download
curl -fsSL "$base/SHA256SUMS" -o "$V/SHA256SUMS"
curl -fsSL "$base/$ASSET" -o "$V/$ASSET"
( cd "$V" && grep " $ASSET\$" SHA256SUMS | sha256sum -c - ) || die "the published $ASSET does not match the published SHA256SUMS"
cmp -s "$V/SHA256SUMS" dist/SHA256SUMS || die "the published SHA256SUMS differs from the local one"
echo "released $TAG: $base/$ASSET"
