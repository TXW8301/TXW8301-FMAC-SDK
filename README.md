# TXW8301-FMAC-SDK
TX_AH SDK 2.4 FMAC firmware development repository.

## Docker Build (TXW8301-14)

This repository includes a fully containerized FMAC firmware build for SDK `v2.4.1.5-40938`, using the vendor Windows CDK under Wine.

### What Is Implemented

- Docker image with Wine 32-bit support (`WINEARCH=win32`, `wine32`, `wine64`)
- CDK V2.8.8 extraction with `unshield` and install into Wine prefix
- Auto-generated GNU Makefile from `project/txw4002a.cdkproj`
- Compile, link, and packaging stages running in container
- Build artifact staging under `project/build/YYYYMMDD_HHMM`
- Automatic cleanup of previous generated artifacts before each run

### Key Files

- `docker/Dockerfile`: toolchain/runtime image
- `docker/scripts/install-cdk.sh`: CDK extraction and wrapper setup
- `docker/scripts/cdkproj_to_makefile.py`: CDK project -> Makefile generator
- `docker/scripts/container-build.sh`: in-container build orchestration
- `docker/run-fmac-docker.sh`: host-side wrapper

### Prerequisites

1. Docker on host (BuildKit-capable).
2. CDK installer extracted at:
   - `SDK/CDK/cdk-windows-V2.8.8-20210621-1740/`

Optional first-time bootstrap (no local CDK extraction yet):

- `CDK_AUTO_FETCH=1`
- `CDK_FTP_URL` set to Taixin FTP zip URL
- `CDK_SHA256` (optional, recommended)

### Run

```bash
cd SDK/TX_AH_SDK_2.4/FMAC/TXW8301_FMAC-v2.4.1.5-40938
./docker/run-fmac-docker.sh
```

Compile only (skip vendor packaging):

```bash
SKIP_PACKAGING=1 ./docker/run-fmac-docker.sh
```

First-time bootstrap from FTP:

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

### Build Outputs

Each run creates a timestamped output folder:

- `project/build/YYYYMMDD_HHMM/`

Typical files in that folder:

- `project.elf`
- `project.hex`
- `project.map`
- `txw8301.bin`
- `param.bin` (when generated)
- `APP.bin` (when generated)
- `txw8301_v2.4.1.5-40938_*.bin`

The script removes previous generated artifacts before each run while keeping source/project files intact.

### Notes

- Vendor packaging batch scripts still print a Windows backup-path warning, but packaging succeeds.
- Generated outputs are ignored via repository `.gitignore`.
