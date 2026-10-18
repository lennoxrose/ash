#!/usr/bin/env bash
# Mnemosyne / Ash toolchain installer. Safe to re-run; installs only what is missing.
#
#   tools/install.sh          install everything
#   tools/install.sh --check  only report what is missing (exit 1 if anything is)
#
# Supports pacman (Arch/CachyOS), apt (Debian/Ubuntu) and dnf (Fedora).
# Rust comes from rustup on every distro (stable + nightly/miri for memory checking).
set -euo pipefail

CHECK=0
[[ "${1:-}" == "--check" ]] && CHECK=1

# command -> package, per package manager. "-" means not packaged: handled below.
#              cmd        pacman           apt                  dnf
TOOLS=(
  "git        git              git                  git"
  "curl       curl             curl                 curl"
  "make       make             make                 make"
  "gcc        gcc              build-essential      gcc"
  "g++        gcc              g++                  gcc-c++"
  "clang      clang            clang                clang"
  "cmake      cmake            cmake                cmake"
  "pkg-config pkgconf          pkg-config           pkgconf-pkg-config"
  "ld         binutils         binutils             binutils"
  "gfortran   gcc-fortran      gfortran             gcc-gfortran"
  "nasm       nasm             nasm                 nasm"
  "zig        zig              -                    zig"
  "gdb        gdb              gdb                  gdb"
  "valgrind   valgrind         valgrind             valgrind"
  "strace     strace           strace               strace"
  "perf       perf             linux-tools-generic  perf"
  "x86_64-w64-mingw32-gcc mingw-w64-gcc gcc-mingw-w64-x86-64 mingw64-gcc"
  "wine       wine             wine64               wine"
  "qemu-system-x86_64 qemu-system-x86 qemu-system-x86   qemu-system-x86"
)

if   command -v pacman  >/dev/null; then PM=pacman; COL=2
elif command -v apt-get >/dev/null; then PM=apt;    COL=3
elif command -v dnf     >/dev/null; then PM=dnf;    COL=4
else echo "unsupported distro: need pacman, apt or dnf" >&2; exit 2; fi

missing_cmds=(); pkgs=()
for row in "${TOOLS[@]}"; do
  read -ra f <<<"$row"
  cmd=${f[0]}; pkg=${f[$((COL-1))]}
  command -v "$cmd" >/dev/null && continue
  missing_cmds+=("$cmd")
  [[ "$pkg" == "-" ]] && { echo "note: $cmd is not packaged for $PM; install it from https://ziglang.org/download" >&2; continue; }
  pkgs+=("$pkg")
done

have_rust() { command -v rustup >/dev/null || [[ -x "$HOME/.cargo/bin/rustup" ]]; }
have_rust || missing_cmds+=("rustup")

if (( CHECK )); then
  if (( ${#missing_cmds[@]} )); then echo "missing: ${missing_cmds[*]}"; exit 1; fi
  echo "all tools present"; exit 0
fi

SUDO=""; [[ $EUID -ne 0 ]] && SUDO="sudo"

if (( ${#pkgs[@]} )); then
  mapfile -t pkgs < <(printf '%s\n' "${pkgs[@]}" | sort -u)
  echo ">> installing: ${pkgs[*]}"
  case $PM in
    pacman) $SUDO pacman -S --needed --noconfirm "${pkgs[@]}" ;;
    apt)    $SUDO apt-get update && $SUDO apt-get install -y "${pkgs[@]}" ;;
    dnf)    $SUDO dnf install -y "${pkgs[@]}" ;;
  esac
fi

# Rust: rustup so nightly + miri (UB / aliasing checker for unsafe code) are available.
export PATH="$HOME/.cargo/bin:$PATH"
if ! command -v rustup >/dev/null; then
  echo ">> installing rustup"
  curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y --profile minimal --default-toolchain stable
fi
rustup component add rustfmt clippy
rustup toolchain install nightly --profile minimal --component miri --component rust-src
command -v cbindgen >/dev/null || cargo install cbindgen   # generates the C ABI header from Rust

echo
echo "done. Add to your shell if not already: export PATH=\"\$HOME/.cargo/bin:\$PATH\""
echo "fish: fish_add_path ~/.cargo/bin"
