#!/usr/bin/env bash
# run-fmac-docker.sh — build the Docker image (with CDK toolchain via Wine)
# and run the containerised FMAC firmware build pipeline.
#
# Requirements:
#   • Docker with BuildKit enabled (default since Docker 23)
#   • CDK installer extracted at SDK/CDK/cdk-windows-V2.8.8-20210621-1740/
#     (override with CDK_DIR env var)
#
# Usage:
#   ./docker/run-fmac-docker.sh
#   SKIP_PACKAGING=1 ./docker/run-fmac-docker.sh   # compile only
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
PROJECT_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)

# SDK root = .../SDK  (3 levels up from TXW8301_FMAC-…)
SDK_ROOT=$(cd "${PROJECT_ROOT}/../../.." && pwd)

# Default CDK installer directory — user moved CDK to SDK/CDK/
CDK_DIR="${CDK_DIR:-${SDK_ROOT}/CDK/cdk-windows-V2.8.8-20210621-1740}"

IMAGE_TAG=${IMAGE_TAG:-txw8301-fmac-sdk:2.4.1.5-40938}

# ── Pre-flight ────────────────────────────────────────────────────────────────
if [[ ! -d "${CDK_DIR}" ]]; then
    echo "ERROR: CDK installer directory not found: ${CDK_DIR}" >&2
    echo "  Set CDK_DIR or extract the zip to SDK/CDK/cdk-windows-V2.8.8-20210621-1740" >&2
    exit 1
fi
if [[ ! -f "${CDK_DIR}/setup.exe" ]]; then
    echo "ERROR: setup.exe not found in CDK_DIR: ${CDK_DIR}" >&2
    exit 1
fi

echo "[run-fmac-docker] CDK dir : ${CDK_DIR}"
echo "[run-fmac-docker] image   : ${IMAGE_TAG}"
echo "[run-fmac-docker] project : ${PROJECT_ROOT}"

# ── Build image ───────────────────────────────────────────────────────────────
# --build-context cdk-installer injects the CDK directory into the build without
# making it part of the main build context (keeps context transfer fast).
DOCKER_BUILDKIT=1 docker build \
    --build-context "cdk-installer=${CDK_DIR}" \
    -f "${PROJECT_ROOT}/docker/Dockerfile" \
    -t "${IMAGE_TAG}" \
    "${PROJECT_ROOT}"

# ── Run build pipeline ────────────────────────────────────────────────────────
echo "[run-fmac-docker] starting build container..."

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
