# TXW8301 FMAC Containerized Build Notes

## Scope

Repository: https://github.com/TXW8301/TXW8301-FMAC-SDK

Goal: run the vendor FMAC firmware build entirely in Docker, including Windows CDK toolchain usage and vendor packaging tools.

## Final Status

Implemented and verified:

- Docker image builds successfully
- CDK V2.8.8 is extracted/installed in container
- Compiler and linker run under Wine
- Makefile is generated from `project/txw4002a.cdkproj`
- Full compile/link succeeds
- Packaging (`BinScript.exe` + `makecode.exe`) succeeds
- Artifacts are staged under `project/build/YYYYMMDD-HHMM`

## Architecture

### Docker Image

`docker/Dockerfile`

- Base: Ubuntu 22.04
- Installs: `wine`, `wine32`, `xvfb`, `unshield`, build tools
- Uses `WINEARCH=win32` for 32-bit CDK executables

### CDK Installation

`docker/scripts/install-cdk.sh`

- Extracts InstallShield payload via `unshield`
- Installs toolchain into Wine prefix paths expected by vendor tools
- Creates Linux wrapper commands for C-SKY tool binaries

### Makefile Generation

`docker/scripts/cdkproj_to_makefile.py`

- Parses `project/txw4002a.cdkproj`
- Generates `project/Makefile.linux`
- Handles include/define/source extraction and linker flags
- Includes linker group handling for vendor static libraries

### Build Orchestration

`docker/scripts/container-build.sh`

- Starts virtual display for Wine (`Xvfb`)
- Does not remove previous generated artifacts; artifacts are preserved in timestamped folders
- Generates Makefile
- Builds firmware (compile + link)
- Runs packaging tools under Wine
	- Stages outputs to `project/build/YYYYMMDD-HHMM`

`docker/run-fmac-docker.sh`

- Builds Docker image with CDK build context
- Runs container with project mounted
- Supports optional host-side CDK bootstrap from FTP for first-time setup

## Artifact Policy

Per-run output folder:

- `project/build/YYYYMMDD-HHMM/`

Typical staged files:

- `project.elf`
- `project.hex`
- `project.map`
- `txw8301.bin`
- `param.bin` (if generated)
- `APP.bin` (if generated)
- `txw8301_v2.4.1.5-40938_*.bin`

Cleanup behavior before each run:

- Previous generated artifacts are preserved; build outputs are left in their timestamped folders under `project/build/`.

## Git Ignore Policy

`.gitignore` in FMAC repo now excludes generated outputs, including:

- `project/build/`
- `project/Obj/`
- `project/Lst/`
- `project/bakup/`
- `project/Makefile.linux`
- `project/project.elf`, `project/project.hex`, `project/project.map`
- `project/APP.bin`, `project/param.bin`, `project/txw8301.bin`, `project/txw8301_*.bin`

## Commands

Run full build:

```bash
# Change into the FMAC repository directory (example)
cd SDK/TX_AH_SDK_2.4/FMAC/<FMAC_REPO_DIR>
./docker/run-fmac-docker.sh
```

First-time bootstrap (no pre-extracted CDK directory):

```bash
CDK_AUTO_FETCH=1 \
CDK_FTP_URL='ftp://<user>:<pass>@<host>/<path>/cdk-windows-V2.8.8-20210621-1740.zip' \
CDK_SHA256='<optional_sha256>' \
./docker/run-fmac-docker.sh
```

Explicit path override:

```bash
CDK_DIR=/abs/path/to/cdk-windows-V2.8.8-20210621-1740 ./docker/run-fmac-docker.sh
```

Compile-only (skip packaging):

```bash
SKIP_PACKAGING=1 ./docker/run-fmac-docker.sh
```

## Known Notes

- Vendor batch script path handling still prints a backup-path warning in Wine output.
- This warning is non-fatal; packaging and firmware generation complete successfully.

## Runner Environment Variables

- `CDK_DIR`: explicit path to extracted CDK directory
- `CDK_AUTO_FETCH`: default `1` (auto-download into `./cdk/` when `CDK_DIR` is missing); set to `0` to disable
- `CDK_FTP_URL`: FTP URL for CDK zip (used when auto-download runs)
- `CDK_ARCHIVE`: local zip cache path (default under `./cdk/` in the repo)
- `CDK_SHA256`: optional checksum verification for downloaded archive
- `CDK_VERSION_DIR`: expected extracted folder name (default `cdk-windows-V2.8.8-20210621-1740`)

Defaults provided in the runner:

- `CDK_FTP_URL` default: `ftp://183.47.14.74/upload/cdk-windows-V2.8.8-20210621-1740.zip`
- `CDK_SHA256` default: `f3b19310c21bfb9597d9ff22f71284bbb880841355a370ba726783130f18993d`
- `CDK_FTP_USER` / `CDK_FTP_PASS` default: `txguest` / `txguest` (example vendor credentials)

Security note: the script provides a convenience default for a public vendor FTP and example credentials. Do NOT commit private credentials to the repository — pass them via environment variables, a CI secret manager, or a protected credentials file. If auto-download fails, the runner prints a message directing developers to https://ol.taixin-semi.com for assistance.

## Related Work

Jira key: `TXW8301-14`
