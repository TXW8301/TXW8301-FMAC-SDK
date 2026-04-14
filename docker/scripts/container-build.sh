#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT=${PROJECT_ROOT:-/work/fmac}
PROJECT_DIR=${PROJECT_DIR:-"${PROJECT_ROOT}/project"}
BUILD_ROOT=${BUILD_ROOT:-"${PROJECT_DIR}/build"}
# Use hyphen-separated timestamp to match repository convention (YYYYMMDD-HHMM)
BUILD_STAMP=${BUILD_STAMP:-$(date +%Y%m%d-%H%M)}
BUILD_DIR="${BUILD_ROOT}/${BUILD_STAMP}"
# CSKY_BIN_DIR can still override the toolchain wrappers installed in the image
CSKY_BIN_DIR=${CSKY_BIN_DIR:-}
CSKY_PREFIX=${CSKY_PREFIX:-csky-elfabiv2-}

log() { printf '[container-build] %s\n' "$*"; }
fail() { printf '[container-build] ERROR: %s\n' "$*" >&2; exit 1; }
require_cmd() { command -v "$1" >/dev/null 2>&1 || fail "missing command: $1"; }

setup_toolchain_path() {
    if [[ -n "${CSKY_BIN_DIR}" ]]; then
        export PATH="${CSKY_BIN_DIR}:${PATH}"
    fi
}

check_toolchain() {
    local cc="${CSKY_PREFIX}gcc"
    if ! command -v "${cc}" >/dev/null 2>&1; then
        fail "toolchain not found: ${cc}. Rebuild Docker image — CDK wine install creates wrappers in /usr/local/bin."
    fi
    log "toolchain: $(${cc} --version 2>&1 | head -1)"
}

check_project_layout() {
    [[ -d "${PROJECT_DIR}" ]] || fail "project directory missing: ${PROJECT_DIR}"
    [[ -f "${PROJECT_DIR}/txw4002a.cdkproj" ]] || fail "missing txw4002a.cdkproj"
    [[ -f "${PROJECT_DIR}/BinScript.exe" ]] || fail "missing BinScript.exe"
    [[ -f "${PROJECT_DIR}/makecode.exe" ]] || fail "missing makecode.exe"
}

clean_previous_artifacts() {
    cd "${PROJECT_DIR}"
    log "preserving previous generated artifacts; build outputs are kept under ${BUILD_ROOT}"
    # Intentionally left blank: do not remove prior outputs. Artifacts are timestamped
    # into ${BUILD_ROOT}/${BUILD_STAMP} so previous runs remain available for debug.
}

prepare_build_dir() {
    mkdir -p "${BUILD_DIR}"
    log "artifact directory: ${BUILD_DIR}"
}

generate_makefile() {
    log "generating Makefile.linux from txw4002a.cdkproj..."
    python3 /usr/local/bin/cdkproj_to_makefile.py \
        "${PROJECT_DIR}/txw4002a.cdkproj" \
        "${PROJECT_DIR}"
    [[ -f "${PROJECT_DIR}/Makefile.linux" ]] || fail "Makefile.linux generation failed"
    log "Makefile.linux generated"
}

run_compile_stage() {
    cd "${PROJECT_DIR}"

    # BeforeMake hook from cdkproj — removes stale object to force rebuild
    [[ -x "./prebuild.sh" ]] && ./prebuild.sh

    if [[ -n "${FMAC_BUILD_CMD:-}" ]]; then
        log "running custom FMAC_BUILD_CMD"
        bash -lc "${FMAC_BUILD_CMD}"
    else
        log "compiling with generated Makefile.linux"
        make -f Makefile.linux -j"$(nproc)" PROJECT_DIR="${PROJECT_DIR}"
    fi

    [[ -f "./Obj/txw4002a.elf" ]] || fail "missing compile artifact: Obj/txw4002a.elf"
    [[ -f "./Obj/txw4002a.ihex" ]] || fail "missing compile artifact: Obj/txw4002a.ihex"
    log "compile artifacts verified"
}

run_packaging_stage() {
    cd "${PROJECT_DIR}"

    if [[ "${SKIP_PACKAGING:-0}" == "1" ]]; then
        log "SKIP_PACKAGING=1, skipping packaging stage"
        return 0
    fi

    require_cmd wine

    # Stage the artifacts that BinScript.exe / makecode.exe expect
    cp ./Obj/txw4002a.elf  project.elf
    cp ./Obj/txw4002a.ihex project.hex
    cp ./Lst/txw4002a.map  project.map 2>/dev/null || true

    log "running vendor packaging tools under Wine"
    wine ./BinScript.exe BinScript.BinScript
    wine ./makecode.exe

    [[ -f "./txw8301.bin" ]] || fail "packaging did not produce txw8301.bin"
    if [[ -f "./param.bin" ]]; then
        log "found param.bin"
    else
        log "param.bin not present; txw8301.bin is the primary firmware artifact"
    fi
    log "packaging stage complete"
}

stage_build_artifacts() {
    cd "${PROJECT_DIR}"

    # Keep Obj/Lst for debugging, but move top-level generated artifacts into
    # build/YYYYMMDD-HHMM to keep project root clean.
    if [[ -f ./project.elf ]]; then
        mv -f ./project.elf "${BUILD_DIR}/project.elf"
    else
        cp -f ./Obj/txw4002a.elf "${BUILD_DIR}/project.elf"
    fi

    if [[ -f ./project.hex ]]; then
        mv -f ./project.hex "${BUILD_DIR}/project.hex"
    else
        cp -f ./Obj/txw4002a.ihex "${BUILD_DIR}/project.hex"
    fi

    if [[ -f ./project.map ]]; then
        mv -f ./project.map "${BUILD_DIR}/project.map"
    elif [[ -f ./Lst/txw4002a.map ]]; then
        cp -f ./Lst/txw4002a.map "${BUILD_DIR}/project.map"
    fi

    shopt -s nullglob
    for f in ./txw8301.bin ./param.bin ./APP.bin ./txw8301_*.bin; do
        [[ -f "${f}" ]] && mv -f "${f}" "${BUILD_DIR}/"
    done
    shopt -u nullglob

    [[ -f "${BUILD_DIR}/txw8301.bin" ]] || fail "staged artifacts missing txw8301.bin"
    log "artifacts staged in ${BUILD_DIR}"
}

start_display() {
    # Wine requires an X display even for CLI compiler tools.
    # Start a minimal virtual framebuffer if DISPLAY is not already set.
    if [[ -z "${DISPLAY:-}" ]]; then
        Xvfb :99 -screen 0 800x600x8 &
        XVFB_PID=$!
        export DISPLAY=:99
        sleep 1
        log "started virtual display :99 (pid ${XVFB_PID})"
    else
        log "using existing display ${DISPLAY}"
        XVFB_PID=
    fi
}

stop_display() {
    if [[ -n "${XVFB_PID:-}" ]]; then
        kill "${XVFB_PID}" 2>/dev/null || true
        wineserver -k 2>/dev/null || true
    fi
}

main() {
    require_cmd bash
    require_cmd make
    require_cmd python3
    start_display
    trap stop_display EXIT
    setup_toolchain_path
    check_toolchain
    check_project_layout
    clean_previous_artifacts
    prepare_build_dir
    generate_makefile
    run_compile_stage
    run_packaging_stage
    stage_build_artifacts
    log "build pipeline finished"
}

main "$@"
