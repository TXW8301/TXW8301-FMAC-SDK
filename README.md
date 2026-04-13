# TXW8301-FMAC-SDK
TX_AH SDK 2.4 FMAC firmware development repository

## Docker Build (TXW8301-14)

This repository now includes a Docker-based FMAC build pipeline for SDK `v2.4.1.5-40938`.

Files:
- `docker/Dockerfile`: base image with Wine and build prerequisites.
- `docker/scripts/container-build.sh`: in-container build pipeline.
- `docker/run-fmac-docker.sh`: host wrapper to build and run the container.

### Prerequisites

1. Install Docker on the host.
2. Provide a C-SKY toolchain on the host and export `CSKY_BIN_DIR` (read-only mounted into container).
3. Provide `FMAC_BUILD_CMD` that performs compile/link and emits:
	- `project/Obj/txw4002a.elf`
	- `project/Obj/txw4002a.ihex`
	- `project/Lst/txw4002a.map`

### Run

```bash
cd /path/to/TXW8301_FMAC-v2.4.1.5-40938
export CSKY_BIN_DIR=/opt/csky/bin
export CSKY_PREFIX=csky-elfabiv2-
export FMAC_BUILD_CMD='your_compile_and_link_command_here'
./docker/run-fmac-docker.sh
```

Optional:
- `SKIP_PACKAGING=1` skips Wine packaging (`BinScript.exe`/`makecode.exe`) for compile-only checks.

### What the container validates

1. C-SKY toolchain binaries are discoverable.
2. Compile stage outputs exist (`ELF`, `IHEX`, `MAP`).
3. Packaging runs under Wine and produces `project/txw8301.bin`.

If packaging succeeds, the SDK build is considered functional for artifact generation in Docker.
