#!/usr/bin/env bash
# Diagnoses the "WARNING: dzn is not a conformant Vulkan implementation"
# and "Dropped Escape call with ulEscapeCode : ..." lines that show up in
# Pyre's dashboard log under WSL2 (dashboard.c pipes the child process's
# raw stdout+stderr into the log view, so driver chatter goes straight
# through).
#
# What these lines actually mean:
#   - WSL2 has no kernel-level GPU passthrough. Vulkan only reaches the
#     GPU by translating calls to D3D12, via Mesa's "dzn" (Dozen) driver.
#   - NVIDIA additionally ships its own native Vulkan ICD into WSL
#     (/usr/lib/wsl/lib/nvidia_icd.json) alongside the D3D12 path. If the
#     loader finds it, it's used instead of dzn and the warning goes away.
#   - AMD and Intel have no such native WSL ICD. dzn is the only Vulkan
#     path available for them, full stop -- the warning is unavoidable
#     and does not indicate a broken or incomplete install.
#
# So this script:
#   1. Confirms Vulkan can actually see a real GPU (not just llvmpipe).
#   2. On AMD/Intel: reports the dzn warning as expected/cosmetic once
#      a real GPU is confirmed present.
#   3. On NVIDIA: if the native ICD manifest exists on disk but isn't in
#      the loader's search path, registers it under /etc/vulkan/icd.d so
#      native NVIDIA Vulkan is used instead of the dzn fallback.
set -euo pipefail

log()  { printf '%s\n' "$*"; }
ok()   { printf '  [ok] %s\n' "$*"; }
warn() { printf '  [!!] %s\n' "$*"; }
info() { printf '      %s\n' "$*"; }

if ! grep -qi microsoft /proc/version 2>/dev/null; then
    warn "This doesn't look like WSL2 (/proc/version has no 'microsoft' in it)."
    warn "fix_wsl2.sh only knows about WSL2's Vulkan-over-D3D12 setup; exiting."
    exit 1
fi

if ! command -v vulkaninfo >/dev/null 2>&1; then
    warn "vulkaninfo not found -- run tools/install_deps.sh first."
    exit 1
fi

log "fix_wsl2.sh: checking Vulkan device visibility under WSL2"
log

VULKANINFO_OUT="$(vulkaninfo --summary 2>&1 || true)"

# Pull out deviceName/driverID/deviceType, three lines per device block.
mapfile -t NAMES     < <(printf '%s\n' "$VULKANINFO_OUT" | grep "deviceName" | sed 's/.*= //')
mapfile -t DRIVERS    < <(printf '%s\n' "$VULKANINFO_OUT" | grep "driverID"   | sed 's/.*= //')
mapfile -t DEVTYPES   < <(printf '%s\n' "$VULKANINFO_OUT" | grep "deviceType" | sed 's/.*= //')

if [ "${#NAMES[@]}" -eq 0 ]; then
    warn "vulkaninfo returned no devices at all. Output was:"
    printf '%s\n' "$VULKANINFO_OUT" | sed 's/^/      /'
    warn "This means WSL2's D3D12 bridge isn't working, not just a Vulkan issue."
    info "From a Windows (not WSL) PowerShell, try:"
    info "  wsl --update"
    info "  wsl --shutdown     (then reopen your WSL terminal)"
    info "Also confirm the Windows GPU driver itself is current (NVIDIA/AMD/Intel"
    info "all need a driver version that supports WSL GPU paravirtualization)."
    exit 1
fi

HAS_REAL_GPU=0
HAS_NVIDIA_DZN=0
for i in "${!NAMES[@]}"; do
    name="${NAMES[$i]}"
    driver="${DRIVERS[$i]:-unknown}"
    devtype="${DEVTYPES[$i]:-unknown}"
    log "GPU$i: $name"
    info "driverID:   $driver"
    info "deviceType: $devtype"

    case "$devtype" in
        *DISCRETE*|*INTEGRATED*) HAS_REAL_GPU=1 ;;
    esac

    if [ "$driver" = "DRIVER_ID_MESA_DOZEN" ] && printf '%s' "$name" | grep -qi nvidia; then
        HAS_NVIDIA_DZN=1
    fi
    log
done

if [ "$HAS_REAL_GPU" -eq 0 ]; then
    warn "Only software/CPU devices showed up (e.g. llvmpipe) -- no real GPU reached"
    warn "Vulkan. That's the actual problem; the dzn warning is not it."
    info "Checking WSL's D3D12 bridge libraries:"
    if [ -d /usr/lib/wsl/lib ]; then
        ls /usr/lib/wsl/lib | sed 's/^/        /'
    else
        warn "/usr/lib/wsl/lib does not exist -- WSL's GPU support isn't wired up."
    fi
    info "From Windows PowerShell: wsl --update, then wsl --shutdown and reopen."
    exit 1
fi

ok "Vulkan sees a real GPU (not just software rasterizer) -- the dashboard warning"
ok "is noise from driver init, not a sign that your GPU setup is broken."
log

if [ "$HAS_NVIDIA_DZN" -eq 1 ]; then
    log "NVIDIA GPU detected but running through dzn/D3D12 instead of NVIDIA's"
    log "native WSL Vulkan ICD. Looking for the native manifest..."

    NATIVE_ICD=""
    for candidate in /usr/lib/wsl/lib/nvidia_icd.json /usr/lib/wsl/lib/*/nvidia_icd.json; do
        [ -f "$candidate" ] && NATIVE_ICD="$candidate" && break
    done

    if [ -z "$NATIVE_ICD" ]; then
        warn "No nvidia_icd.json found under /usr/lib/wsl/lib."
        info "That file is shipped by the Windows NVIDIA driver, not this Linux"
        info "distro. On Windows: update the NVIDIA driver, run 'wsl --update',"
        info "then 'wsl --shutdown' and reopen WSL. If it's still missing after"
        info "that, dzn is genuinely the only path available right now and the"
        info "warning is cosmetic, same as the AMD/Intel case above."
        exit 0
    fi

    ok "Found native ICD manifest: $NATIVE_ICD"

    if [ "$(id -u)" -ne 0 ]; then
        warn "Need root to register it under /etc/vulkan/icd.d -- re-run with sudo."
        exit 1
    fi

    mkdir -p /etc/vulkan/icd.d
    DEST=/etc/vulkan/icd.d/nvidia_icd.json
    if [ -e "$DEST" ] && [ ! -L "$DEST" ]; then
        warn "$DEST already exists and isn't a symlink we made -- leaving it alone."
        info "Point VK_ICD_FILENAMES=$NATIVE_ICD at it manually if you want to force it."
        exit 1
    fi
    ln -sf "$NATIVE_ICD" "$DEST"
    ok "Linked $DEST -> $NATIVE_ICD"

    log
    log "Re-checking with the native ICD registered:"
    if vulkaninfo --summary 2>/dev/null | grep -q "DRIVER_ID_NVIDIA"; then
        ok "Native NVIDIA driver is now active -- the dzn warning should be gone."
    else
        warn "Still not picked up. Try forcing it for one run to confirm the file works:"
        info "  VK_ICD_FILENAMES=$NATIVE_ICD vulkaninfo --summary"
    fi
    exit 0
fi

log "No native WSL Vulkan ICD exists for this GPU vendor (AMD/Intel don't have"
log "one, unlike NVIDIA) -- dzn-over-D3D12 is the only Vulkan path WSL2 gives"
log "you here. The warnings you're seeing are expected driver chatter on every"
log "Vulkan app under WSL2 with this hardware, including Pyre's own benchmarks."
log "There is nothing left to install or fix."
