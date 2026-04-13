#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT=${PROJECT_ROOT:-/work}
PROJECT_DIR=${PROJECT_DIR:-"${PROJECT_ROOT}/project"}
CSKY_BIN_DIR=${CSKY_BIN_DIR:-}
CSKY_PREFIX=${CSKY_PREFIX:-csky-elfabiv2-}

log() {
    printf '[container-build] %s\n' "$*"
}

fail() {
    printf '[container-build] ERROR: %s\n' "$*" >&2
    exit 1
}

require_cmd() {
    command -v "$1" >/dev/null 2>&1 || fail "missing command: $1"
}

setup_toolchain_path() {
    if [[ -n "${CSKY_BIN_DIR}" ]]; then
        export PATH="${CSKY_BIN_DIR}:${PATH}"
    fi
}

check_toolchain() {
    local cc="${CSKY_PREFIX}gcc"
    local objcopy="${CSKY_PREFIX}objcopy"
    local objdump="${CSKY_PREFIX}objdump"

    if ! command -v "${cc}" >/dev/null 2>&1; then
        fail "toolchain compiler not found: ${cc}. Set CSKY_BIN_DIR and/or CSKY_PREFIX."
    fi
    if ! command -v "${objcopy}" >/dev/null 2>&1; then
        fail "toolchain objcopy not found: ${objcopy}."
    fi
    if ! command -v "${objdump}" >/dev/null 2>&1; then
        fail "toolchain objdump not found: ${objdump}."
    fi

    log "toolchain detected: ${cc}"
}

check_project_layout() {
    [[ -d "${PROJECT_DIR}" ]] || fail "project directory missing: ${PROJECT_DIR}"
    [[ -f "${PROJECT_DIR}/txw4002a.cdkproj" ]] || fail "missing txw4002a.cdkproj"
    [[ -f "${PROJECT_DIR}/BuildBIN.sh" ]] || fail "missing BuildBIN.sh"
    [[ -f "${PROJECT_DIR}/BinScript.exe" ]] || fail "missing BinScript.exe"
    [[ -f "${PROJECT_DIR}/makecode.exe" ]] || fail "missing makecode.exe"
}

run_compile_stage() {
    cd "${PROJECT_DIR}"

    if [[ -n "${FMAC_BUILD_CMD:-}" ]]; then
        log "running FMAC_BUILD_CMD"
        bash -lc "${FMAC_BUILD_CMD}"
    elif [[ -x "./prebuild.sh" ]]; then
        log "running prebuild.sh"
        ./prebuild.sh
        fail "compile command not provided. Set FMAC_BUILD_CMD to run the SDK compile/link stage."
    else
        fail "no compile command path available"
    fi

    [[ -f "./Obj/txw4002a.elf" ]] || fail "missing compile artifact: Obj/txw4002a.elf"
    [[ -f "./Obj/txw4002a.ihex" ]] || fail "missing compile artifact: Obj/txw4002a.ihex"
    [[ -f "./Lst/txw4002a.map" ]] || fail "missing compile artifact: Lst/txw4002a.map"

    log "compile artifacts detected"
}

run_packaging_stage() {
    cd "${PROJECT_DIR}"

    if [[ "${SKIP_PACKAGING:-0}" == "1" ]]; then
        log "SKIP_PACKAGING=1, skipping packaging stage"
        return 0
    fi

    require_cmd wine

    log "running vendor packaging tools under Wine"
    wine ./BinScript.exe BinScript.BinScript
    wine ./makecode.exe

    [[ -f "./txw8301.bin" ]] || fail "packaging did not produce txw8301.bin"

    # makecode flow may emit parameter binary in different names across SDK drops.
    if [[ -f "./param.bin" ]]; then
        log "found param.bin"
    else
        log "param.bin not found; continue with txw8301.bin as primary firmware artifact"
    fi

    log "packaging stage completed"
}

main() {
    require_cmd bash
    setup_toolchain_path
    check_toolchain
    check_project_layout
    run_compile_stage
    run_packaging_stage
    log "build pipeline finished"
}

main "$@"
