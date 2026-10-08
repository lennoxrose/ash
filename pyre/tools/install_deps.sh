#!/usr/bin/env bash
# Installs everything Pyre's GPU backend needs to build (not necessarily
# run -- that also needs an actual GPU + driver, which this script
# can't provide). Vulkan headers/loader, the SPIR-V shader compiler
# toolchain (glslc, glslangValidator), spirv-tools, and vulkan-tools
# (vulkaninfo, for checking what the machine actually has once a real
# driver is installed).
#
# Arch Linux (pacman) only for now -- this is what both the sandbox this
# was developed in and the user's own machine run. Add another package
# manager's branch here if Pyre ever needs to build somewhere else;
# don't guess package names for a distro nobody has actually run this
# against yet.
set -euo pipefail

if ! command -v pacman >/dev/null 2>&1; then
    echo "install_deps.sh: this script only knows pacman (Arch Linux)." >&2
    echo "Install these packages' equivalents by hand on your distro:" >&2
    echo "  vulkan-headers vulkan-icd-loader vulkan-tools shaderc glslang spirv-tools" >&2
    exit 1
fi

PACMAN_PKGS=(vulkan-headers vulkan-icd-loader vulkan-tools shaderc glslang spirv-tools)

echo "install_deps.sh: pacman -Sy then installing: ${PACMAN_PKGS[*]}"
pacman -Sy --noconfirm
pacman -S --noconfirm --needed "${PACMAN_PKGS[@]}"

echo
echo "install_deps.sh: done. Verifying:"
command -v glslc >/dev/null 2>&1 && echo "  glslc: $(glslc --version | head -1)" || echo "  glslc: MISSING"
[ -f /usr/include/vulkan/vulkan.h ] && echo "  vulkan headers: present" || echo "  vulkan headers: MISSING"
command -v vulkaninfo >/dev/null 2>&1 && echo "  vulkaninfo: present" || echo "  vulkaninfo: MISSING"
echo
echo "This installs the SDK/loader, not a GPU driver. 'vulkaninfo --summary'"
echo "still needs a real vendor driver (vulkan-radeon for an AMD card like"
echo "the RX 9070 XT, vulkan-intel, nvidia-utils, ...) installed separately"
echo "-- 'Found no drivers!' from vulkaninfo on a machine with no GPU (like"
echo "the sandbox this was written in) is the correct, honest answer, not"
echo "a failure of this script."
