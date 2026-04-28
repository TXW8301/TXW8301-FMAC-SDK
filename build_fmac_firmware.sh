#!/usr/bin/env bash
# build_fmac_firmware.sh — Build TXW8301 FMAC firmware for a specific bus interface.
#
# Usage:
#   ./build_fmac_firmware.sh -sdio
#   ./build_fmac_firmware.sh -usb
#   ./build_fmac_firmware.sh -uart
#
# The baseline project_config.h is NEVER modified. A patched copy is mounted
# into the Docker container at build time so only the chosen MACBUS define is
# active. After the Docker build the output binary is renamed with a bus-mode
# suffix, e.g.:
#   txw8301_vX.Y.Z-BBBBB_2026.4.28_usb.bin

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# ── Paths ──────────────────────────────────────────────────────────────────
FMAC_DIR="$SCRIPT_DIR"
PROJECT_CONFIG="$FMAC_DIR/project/project_config.h"
DOCKER_IMAGE="txw8301-fmac-sdk:2.4.1.5-40938"
# ───────────────────────────────────────────────────────────────────────────

usage() {
    cat <<EOF
Usage: $0 [-sdio | -usb | -uart]

Build TXW8301 FMAC firmware for the given bus interface using Docker.
The baseline project_config.h is never modified.

Run from: SDK/TX_AH_SDK_2.4/FMAC/TXW8301_FMAC/
Output  : project/build/<timestamp>/txw8301_vX.Y.Z-BBBBB_<date>_<mode>.bin
EOF
    exit 1
}

BUILD_MODE=""
for arg in "$@"; do
    case "$arg" in
        -sdio) BUILD_MODE="sdio" ;;
        -usb)  BUILD_MODE="usb"  ;;
        -uart) BUILD_MODE="uart" ;;
        -h|--help) usage ;;
        *) echo "ERROR: Unknown option: $arg" >&2; usage ;;
    esac
done

[[ -z "$BUILD_MODE" ]] && { echo "ERROR: Bus mode required." >&2; usage; }

echo "==> FMAC firmware build  [mode=${BUILD_MODE}]"
echo "    Source : $FMAC_DIR"

# ── 1. Clean stale Obj/ ────────────────────────────────────────────────────
# Must be removed before a new build to avoid stale objects from a different
# bus configuration being silently reused (SDK Makefile uses mtime, not config
# dependency tracking).
OBJ_DIR="$FMAC_DIR/project/Obj"
if [[ -d "$OBJ_DIR" ]]; then
    echo "==> Removing $OBJ_DIR ..."
    rm -rf "$OBJ_DIR"
fi

# ── 2. Generate patched project_config.h (temp file, baseline untouched) ───
TEMP_CONFIG="$(mktemp /tmp/project_config_XXXXXX.h)"
trap 'rm -f "$TEMP_CONFIG"' EXIT

# Normalise all three MACBUS lines to commented-out form.
# [/]* matches zero or more leading slashes, so this handles both
# "#define MACBUS_SDIO" (active) and "//#define MACBUS_SDIO" (disabled).
sed \
    -e 's|^[/]*#define MACBUS_SDIO[[:space:]]*$|//#define MACBUS_SDIO|' \
    -e 's|^[/]*#define MACBUS_USB[[:space:]]*$|//#define MACBUS_USB|'   \
    -e 's|^[/]*#define MACBUS_UART[[:space:]]*$|//#define MACBUS_UART|' \
    "$PROJECT_CONFIG" > "$TEMP_CONFIG"

# Enable only the chosen bus.
case "$BUILD_MODE" in
    sdio) sed -i 's|^//#define MACBUS_SDIO$|#define MACBUS_SDIO|' "$TEMP_CONFIG" ;;
    usb)  sed -i 's|^//#define MACBUS_USB$|#define MACBUS_USB|'   "$TEMP_CONFIG" ;;
    uart) sed -i 's|^//#define MACBUS_UART$|#define MACBUS_UART|' "$TEMP_CONFIG" ;;
esac

echo "==> Effective config (patched header, not written to disk):"
grep "define MACBUS" "$TEMP_CONFIG"

# ── 3. Docker build ────────────────────────────────────────────────────────
# The Docker image bakes the caller's UID/GID into the wineprefix ownership
# (via BUILD_UID/BUILD_GID args in build-fmac-image.sh).  --user passes the
# same UID at run time, satisfying Wine's ownership check.  Output files are
# created owned by the calling user directly — no sudo required.
echo "==> Running Docker build..."
docker run --rm \
    --user "$(id -u):$(id -g)" \
    -e PROJECT_ROOT=/work/fmac \
    -v "$FMAC_DIR:/work/fmac" \
    -v "$TEMP_CONFIG:/work/fmac/project/project_config.h:ro" \
    "$DOCKER_IMAGE"

# ── 4. Find freshly built binary and tag with bus mode ─────────────────────
BUILD_OUT_DIR="$FMAC_DIR/project/build"

# Primary: binaries created after our temp config (i.e., during this run).
LATEST_BIN="$(find "$BUILD_OUT_DIR" -maxdepth 2 -name "*.bin" -newer "$TEMP_CONFIG" 2>/dev/null \
    | sort | tail -1)"

# Fallback: most recently touched binary in the build tree.
if [[ -z "$LATEST_BIN" ]]; then
    LATEST_BIN="$(find "$BUILD_OUT_DIR" -maxdepth 2 -name "*.bin" 2>/dev/null \
        | xargs ls -t 2>/dev/null | head -1 || true)"
fi

if [[ -z "$LATEST_BIN" ]]; then
    echo "ERROR: No output binary found under $BUILD_OUT_DIR" >&2
    exit 1
fi

# Rename: txw8301_.._.bin  →  txw8301_.._<mode>.bin
# The SDK produces a trailing underscore before .bin, e.g. "..._2026.4.28_.bin"
if [[ "$LATEST_BIN" == *_.bin ]]; then
    DEST_BIN="${LATEST_BIN%_.bin}_${BUILD_MODE}.bin"
else
    DEST_BIN="${LATEST_BIN%.bin}_${BUILD_MODE}.bin"
fi

# Rename and remove the untagged original.
cp "$LATEST_BIN" "$DEST_BIN"
rm "$LATEST_BIN"

echo ""
echo "==> Build complete."
printf "    Output : %s\n"   "$DEST_BIN"
printf "    Size   : %d bytes\n" "$(stat -c%s "$DEST_BIN")"
printf "    MD5    : %s\n"   "$(md5sum "$DEST_BIN" | cut -d' ' -f1)"
