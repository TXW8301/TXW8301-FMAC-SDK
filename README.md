# TXW8301-FMAC-SDK
TX_AH SDK 2.4 FMAC firmware development repository.

## Docker Build (TXW8301-14)

This repository includes a fully containerized FMAC firmware build for SDK `v2.4.1.5-40938`, using the vendor Windows CDK under Wine.

## Why use the Docker build

The vendor CDK (C-SKY MinGW toolchain) and the firmware packaging tools are distributed as Windows executables. The Docker image runs the vendor CDK inside a Wine prefix and provides Wine wrapper scripts so the exact Windows toolchain and packaging flow run unchanged on Linux and in CI. This approach was chosen because it:

- Ensures reproducible builds: the same compiler, linker and packaging tools for all developers and CI.
- Removes the need for a Windows development host: the Windows CDK runs under Wine inside the container.
- Uses vendor packaging tools unchanged: produces firmware images exactly as the vendor tools expect.
- Keeps the host clean and portable: CDK and generated artifacts are isolated in the container and staged into timestamped build folders.
- Is CI-friendly and automatable: supports repeatable releases and the optional CDK auto-bootstrap for first-time setup.

Caveats:
- You still need the vendor CDK installer (place it under `./cdk` in this repo or use the `CDK_AUTO_FETCH` option).
- The build relies on Wine-wrapped toolchain semantics; behavior may differ slightly from native Linux cross-compilers — see DOCKER_BUILD_NOTES for details.

### What Is Implemented

- Docker image with Wine 32-bit support (`WINEARCH=win32`, `wine32`, `wine64`)
- CDK V2.8.8 extraction with `unshield` and install into Wine prefix
- Auto-generated GNU Makefile from `project/txw4002a.cdkproj`
- Compile, link, and packaging stages running in container
- Build artifact staging under `project/build/YYYYMMDD-HHMM`

### Key Files

- `docker/Dockerfile`: toolchain/runtime image
- `docker/scripts/install-cdk.sh`: CDK extraction and wrapper setup
- `docker/scripts/cdkproj_to_makefile.py`: CDK project -> Makefile generator
- `docker/scripts/container-build.sh`: in-container build orchestration
- `docker/run-fmac-docker.sh`: host-side wrapper

### Prerequisites

1. Docker on host (BuildKit-capable).
2. CDK installer extracted at (default):
   - `./cdk/cdk-windows-V2.8.8-20210621-1740/` (inside this FMAC repo)

If no CDK is present, the runner will auto-download the CDK into `./cdk/` by default to simplify first-time onboarding. To disable automatic download set `CDK_AUTO_FETCH=0`.

- `CDK_FTP_URL` set to Taixin FTP zip URL (default: `ftp://183.47.14.74/upload/cdk-windows-V2.8.8-20210621-1740.zip`)
- `CDK_FTP_USER` / `CDK_FTP_PASS`: optional FTP credentials (example: `txguest` / `txguest`)
- `CDK_SHA256` (recommended): `f3b19310c21bfb9597d9ff22f71284bbb880841355a370ba726783130f18993d`

Note: the script includes a default FTP URL and an example credential pair for convenience. Do NOT commit private credentials into the repository; prefer passing credentials via environment variables or a secrets manager.

### Run

```bash
# Change into the FMAC repository directory (example)
cd SDK/TX_AH_SDK_2.4/FMAC/<FMAC_REPO_DIR>
./docker/run-fmac-docker.sh
```

Compile only (skip vendor packaging):

```bash
SKIP_PACKAGING=1 ./docker/run-fmac-docker.sh
```

First-time bootstrap from FTP (example):

```bash
CDK_AUTO_FETCH=1 \
CDK_FTP_URL='ftp://183.47.14.74/upload/cdk-windows-V2.8.8-20210621-1740.zip' \
CDK_FTP_USER='txguest' \
CDK_FTP_PASS='txguest' \
CDK_SHA256='f3b19310c21bfb9597d9ff22f71284bbb880841355a370ba726783130f18993d' \
./docker/run-fmac-docker.sh
```

Explicit path override:

```bash
CDK_DIR=/abs/path/to/cdk-windows-V2.8.8-20210621-1740 ./docker/run-fmac-docker.sh
```

### Build Outputs

Each run creates a timestamped output folder:

- `project/build/YYYYMMDD-HHMM/`

Typical files in that folder:

- `project.elf`
- `project.hex`
- `project.map`
- `txw8301.bin`
- `param.bin` (when generated)
- `APP.bin` (when generated)
- `txw8301_v2.4.1.5-40938_*.bin`

Generated artifacts are preserved in timestamped build folders; source/project files are left intact.

### Notes

- Vendor packaging batch scripts still print a Windows backup-path warning, but packaging succeeds.
- Generated outputs are ignored via repository `.gitignore`.
