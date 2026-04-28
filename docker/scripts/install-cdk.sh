#!/usr/bin/env bash
# install-cdk.sh — Extract the CDK toolchain from the InstallShield package
# using unshield (no Windows required), install it into the Wine C: drive at
# the path GCC expects, and create Linux wrapper scripts for every
# csky-elfabiv2-* compiler tool.
#
# Why the Wine C: drive path matters:
#   GCC uses GetModuleFileName() at startup to find its own Windows path, then
#   derives the prefix for cc1.exe, libgcc.a, and system headers from it.
#   The original install path embedded in the CDK binaries is:
#     C:\CSKY\MinGW\csky-abiv2-elf-toolchain\
#   So we copy the toolchain to exactly that location in the Wine prefix so
#   GCC can locate cc1.exe, collect2.exe, and the GCC 6.3.0 support libraries.
#
# Run during Docker image build with CDK installer at /tmp/cdk-setup/.
set -euo pipefail

CDK_SETUP_DIR=/tmp/cdk-setup
CDK_EXTRACT_DIR=/tmp/cdk-extracted

# Wine C: drive location that mirrors the CDK Windows install layout.
# GCC expects to be at C:\CSKY\MinGW\csky-abiv2-elf-toolchain\.
WINE_C="${WINEPREFIX:-/opt/cdk-wineprefix}/drive_c"
WINE_TOOLCHAIN="${WINE_C}/CSKY/MinGW/csky-abiv2-elf-toolchain"

export WINEPREFIX="${WINEPREFIX:-/opt/cdk-wineprefix}"
export WINEDEBUG=-all

log()  { printf '[install-cdk] %s\n' "$*"; }
warn() { printf '[install-cdk] WARNING: %s\n' "$*"; }
fail() { printf '[install-cdk] ERROR: %s\n' "$*" >&2; exit 1; }

[[ -f "${CDK_SETUP_DIR}/data1.hdr" ]] || fail "data1.hdr not found — CDK setup directory is empty"

# ── Extract CDK payload via unshield ────────────────────────────────────────
# unshield reads the InstallShield package natively on Linux without running
# the installer. data1.hdr is the index; data2.cab contains the payload.
# unshield reads both automatically when given data1.hdr.
log "extracting CDK via unshield (622 MB — this may take several minutes)..."
mkdir -p "${CDK_EXTRACT_DIR}"
unshield -d "${CDK_EXTRACT_DIR}" x "${CDK_SETUP_DIR}/data1.hdr" 2>&1 | tail -3 || true

# ── Locate extracted toolchain tree ─────────────────────────────────────────
# unshield creates: CDK_EXTRACT_DIR/<component>/CSKY/MinGW/csky-abiv2-elf-toolchain/
# The component directory is named "root_directory" by the InstallShield project.
log "locating csky-abiv2-elf-toolchain in extracted content..."
TOOLCHAIN_SRC=$(find "${CDK_EXTRACT_DIR}" -type d -name "csky-abiv2-elf-toolchain" 2>/dev/null | head -1)

if [[ -z "${TOOLCHAIN_SRC}" ]]; then
    log "Extracted top-level directories:"
    find "${CDK_EXTRACT_DIR}" -maxdepth 6 -type d 2>/dev/null | head -30 || true
    fail "csky-abiv2-elf-toolchain directory not found after extraction"
fi
log "found source: ${TOOLCHAIN_SRC}"

# ── Install toolchain into Wine C: drive ────────────────────────────────────
# Place at the exact Windows install path so GCC can find cc1.exe and libgcc.a
# based on its own executable location.
log "installing toolchain into Wine prefix at ${WINE_TOOLCHAIN}..."
mkdir -p "$(dirname "${WINE_TOOLCHAIN}")"
cp -a "${TOOLCHAIN_SRC}" "${WINE_TOOLCHAIN}"
log "toolchain size: $(du -sh "${WINE_TOOLCHAIN}" | cut -f1)"

# ── Copy required MinGW runtime DLLs ────────────────────────────────────────
# The toolchain EXEs are MinGW-compiled and may depend on libgcc_s_dw2-1.dll
# which ships in CSKY\Git\mingw32\bin\. Copy it alongside the toolchain bin.
MINGW32_BIN=$(find "${CDK_EXTRACT_DIR}" -path "*/Git/mingw32/bin" -type d 2>/dev/null | head -1)
if [[ -n "${MINGW32_BIN}" ]]; then
    log "copying MinGW32 runtime DLLs..."
    cp -v "${MINGW32_BIN}"/*.dll "${WINE_TOOLCHAIN}/bin/" 2>/dev/null || true
fi

# ── Clean up extraction directory ───────────────────────────────────────────
log "cleaning up extraction directory (freeing disk space)..."
rm -rf "${CDK_EXTRACT_DIR}"

# ── Create Linux wrapper scripts ─────────────────────────────────────────────
# Wrappers live in /usr/local/bin so the build system can call them directly.
# DISPLAY defaults to :99 (callers must start Xvfb :99 before invoking make).
log "creating toolchain wrapper scripts..."
WRAPPER_BIN="${WINE_TOOLCHAIN}/bin"
WRAPPERS_CREATED=0

for EXE in "${WRAPPER_BIN}"/csky-elfabiv2-*.exe; do
    [[ -f "${EXE}" ]] || continue
    TOOL=$(basename "${EXE}" .exe)
    WRAPPER=/usr/local/bin/${TOOL}
    # Use printf to avoid heredoc variable-expansion issues in generated scripts
    printf '#!/bin/sh\nexport WINEPREFIX=%s\nexport WINEDEBUG=-all\n: ${DISPLAY:=:99}\nexport DISPLAY\nexec wine "%s" "$@"\n' \
        "${WINEPREFIX}" "${EXE}" > "${WRAPPER}"
    chmod +x "${WRAPPER}"
    WRAPPERS_CREATED=$((WRAPPERS_CREATED + 1))
done

log "created ${WRAPPERS_CREATED} wrappers in /usr/local/bin"
[[ "${WRAPPERS_CREATED}" -gt 0 ]] || fail "no wrapper scripts created — check extraction"

# ── Record toolchain bin path for diagnostics ────────────────────────────────
echo "${WRAPPER_BIN}" > /etc/csky-compiler-dir

# ── Smoke-test ───────────────────────────────────────────────────────────────
log "starting virtual display for Wine smoke test..."
Xvfb :90 -screen 0 800x600x8 &
XVFB_PID=$!
export DISPLAY=:90
sleep 2   # give Xvfb time to start

log "smoke-testing csky-elfabiv2-gcc --version..."
SMOKE_OUT=$(csky-elfabiv2-gcc --version 2>&1 || true)
if echo "${SMOKE_OUT}" | grep -Eqi "csky|gcc|6\.3|elfabiv2"; then
    log "compiler OK: $(echo "${SMOKE_OUT}" | head -1)"
else
    warn "unexpected output from --version (may still work at build time):"
    echo "${SMOKE_OUT}" | head -5 || true
fi

kill "${XVFB_PID}" 2>/dev/null || true
wineserver -k 2>/dev/null || true
# Remove root-owned Wine socket dirs so they are not baked into the image
# layer; the builder user needs to create its own socket dir at run time.
rm -rf /tmp/.wine-* /tmp/wine-* 2>/dev/null || true
log "CDK toolchain setup complete — wrappers in /usr/local/bin"
