#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
PROJECT_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)
IMAGE_TAG=${IMAGE_TAG:-txw8301-fmac-sdk:2.4.1.5-40938}

if [[ -z "${CSKY_BIN_DIR:-}" ]]; then
    echo "ERROR: CSKY_BIN_DIR is not set. Example: /opt/csky/bin" >&2
    exit 1
fi

if [[ -z "${FMAC_BUILD_CMD:-}" ]]; then
    echo "ERROR: FMAC_BUILD_CMD is not set. This should run compile/link and create Obj/txw4002a.elf, Obj/txw4002a.ihex, and Lst/txw4002a.map" >&2
    exit 1
fi

echo "[run-fmac-docker] building image ${IMAGE_TAG}"
docker build -f "${PROJECT_ROOT}/docker/Dockerfile" -t "${IMAGE_TAG}" "${PROJECT_ROOT}"

echo "[run-fmac-docker] running container build pipeline"
docker run --rm \
    -v "${PROJECT_ROOT}:/work" \
    -v "${CSKY_BIN_DIR}:${CSKY_BIN_DIR}:ro" \
    -e PROJECT_ROOT=/work \
    -e PROJECT_DIR=/work/project \
    -e CSKY_BIN_DIR="${CSKY_BIN_DIR}" \
    -e CSKY_PREFIX="${CSKY_PREFIX:-csky-elfabiv2-}" \
    -e FMAC_BUILD_CMD="${FMAC_BUILD_CMD}" \
    -e SKIP_PACKAGING="${SKIP_PACKAGING:-0}" \
    "${IMAGE_TAG}"
