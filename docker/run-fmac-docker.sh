#!/usr/bin/env bash
# run-fmac-docker.sh — build the Docker image (with CDK toolchain via Wine)
# and run the containerised FMAC firmware build pipeline.
#
# Requirements:
#   • Docker with BuildKit enabled (default since Docker 23)
#   • CDK installer extracted at SDK/CDK/cdk-windows-V2.8.8-20210621-1740/
#     (override with CDK_DIR env var)
#   • Optional bootstrap for new developers:
#       CDK_AUTO_FETCH=1 + CDK_FTP_URL can download/extract CDK automatically
#
# Usage:
#   ./docker/run-fmac-docker.sh
#   SKIP_PACKAGING=1 ./docker/run-fmac-docker.sh   # compile only
#
# Auto-fetch example:
#   CDK_AUTO_FETCH=1 \
#   CDK_FTP_URL='ftp://user:pass@example.com/path/cdk-windows-V2.8.8-20210621-1740.zip' \
#   ./docker/run-fmac-docker.sh
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
PROJECT_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)

# SDK root = .../SDK  (3 levels up from TXW8301_FMAC-…)
SDK_ROOT=$(cd "${PROJECT_ROOT}/../../.." && pwd)
SDK_CDK_ROOT="${SDK_ROOT}/CDK"
CDK_VERSION_DIR="${CDK_VERSION_DIR:-cdk-windows-V2.8.8-20210621-1740}"

# Default CDK installer directory — user moved CDK to SDK/CDK/
CDK_DIR="${CDK_DIR:-${SDK_CDK_ROOT}/${CDK_VERSION_DIR}}"
CDK_ARCHIVE="${CDK_ARCHIVE:-${SDK_CDK_ROOT}/${CDK_VERSION_DIR}.zip}"
CDK_AUTO_FETCH="${CDK_AUTO_FETCH:-0}"
CDK_FTP_URL="${CDK_FTP_URL:-}"
CDK_SHA256="${CDK_SHA256:-}"

IMAGE_TAG=${IMAGE_TAG:-txw8301-fmac-sdk:2.4.1.5-40938}

log() { printf '[run-fmac-docker] %s\n' "$*"; }
warn() { printf '[run-fmac-docker] WARNING: %s\n' "$*"; }
fail() { printf '[run-fmac-docker] ERROR: %s\n' "$*" >&2; exit 1; }

download_file() {
    local url="$1"
    local out="$2"
    local tmp="${out}.part"

    if command -v curl >/dev/null 2>&1; then
        curl -fL --retry 3 --retry-delay 2 -o "${tmp}" "${url}"
    elif command -v wget >/dev/null 2>&1; then
        wget -O "${tmp}" "${url}"
    else
        fail "neither curl nor wget found on host; cannot auto-download CDK"
    fi

    mv -f "${tmp}" "${out}"
}

extract_zip() {
    local archive="$1"
    local out_dir="$2"

    if command -v unzip >/dev/null 2>&1; then
        unzip -q "${archive}" -d "${out_dir}"
    elif command -v python3 >/dev/null 2>&1; then
        python3 - "${archive}" "${out_dir}" <<'PY'
import os
import sys
import zipfile

archive, out_dir = sys.argv[1], sys.argv[2]
os.makedirs(out_dir, exist_ok=True)
with zipfile.ZipFile(archive, 'r') as zf:
    zf.extractall(out_dir)
PY
    else
        fail "neither unzip nor python3 found on host; cannot extract CDK zip"
    fi
}

bootstrap_cdk_if_needed() {
    [[ -d "${CDK_DIR}" ]] && return 0

    [[ "${CDK_AUTO_FETCH}" == "1" ]] || return 0
    [[ -n "${CDK_FTP_URL}" ]] || fail "CDK_AUTO_FETCH=1 requires CDK_FTP_URL"

    mkdir -p "${SDK_CDK_ROOT}"

    if [[ ! -f "${CDK_ARCHIVE}" ]]; then
        log "CDK not found; downloading archive from FTP..."
        download_file "${CDK_FTP_URL}" "${CDK_ARCHIVE}"
    else
        log "using existing CDK archive: ${CDK_ARCHIVE}"
    fi

    if [[ -n "${CDK_SHA256}" ]]; then
        log "verifying CDK archive checksum..."
        echo "${CDK_SHA256}  ${CDK_ARCHIVE}" | sha256sum -c -
    else
        warn "CDK_SHA256 not set; skipping checksum verification"
    fi

    log "extracting CDK archive to ${SDK_CDK_ROOT}..."
    extract_zip "${CDK_ARCHIVE}" "${SDK_CDK_ROOT}"

    if [[ ! -d "${CDK_DIR}" ]]; then
        local detected
        detected=$(find "${SDK_CDK_ROOT}" -mindepth 1 -maxdepth 1 -type d -name 'cdk-windows-*' | sort | tail -1)
        if [[ -n "${detected}" ]]; then
            warn "expected ${CDK_DIR}, detected ${detected}; using detected path"
            CDK_DIR="${detected}"
        fi
    fi
}

# ── Pre-flight ────────────────────────────────────────────────────────────────
bootstrap_cdk_if_needed

if [[ ! -d "${CDK_DIR}" ]]; then
    fail "CDK installer directory not found: ${CDK_DIR}
  Set CDK_DIR, or set CDK_AUTO_FETCH=1 and CDK_FTP_URL"
fi
if [[ ! -f "${CDK_DIR}/setup.exe" ]]; then
    fail "setup.exe not found in CDK_DIR: ${CDK_DIR}"
fi

log "CDK dir : ${CDK_DIR}"
log "image   : ${IMAGE_TAG}"
log "project : ${PROJECT_ROOT}"

# ── Build image ───────────────────────────────────────────────────────────────
# --build-context cdk-installer injects the CDK directory into the build without
# making it part of the main build context (keeps context transfer fast).
DOCKER_BUILDKIT=1 docker build \
    --build-context "cdk-installer=${CDK_DIR}" \
    -f "${PROJECT_ROOT}/docker/Dockerfile" \
    -t "${IMAGE_TAG}" \
    "${PROJECT_ROOT}"

# ── Run build pipeline ────────────────────────────────────────────────────────
log "starting build container..."

# Mount the entire FMAC dir at /work so relative paths (../libs, ../sdk) resolve
DOCKER_RUN_ARGS=(
    --rm
    -v "${PROJECT_ROOT}:/work/fmac:z"
    -e PROJECT_ROOT=/work/fmac
    -e PROJECT_DIR=/work/fmac/project
    -e "CSKY_PREFIX=${CSKY_PREFIX:-csky-elfabiv2-}"
    -e "SKIP_PACKAGING=${SKIP_PACKAGING:-0}"
)

# Pass FMAC_BUILD_CMD only if the caller set it (allows custom override)
if [[ -n "${FMAC_BUILD_CMD:-}" ]]; then
    DOCKER_RUN_ARGS+=(-e "FMAC_BUILD_CMD=${FMAC_BUILD_CMD}")
fi

docker run "${DOCKER_RUN_ARGS[@]}" "${IMAGE_TAG}"
