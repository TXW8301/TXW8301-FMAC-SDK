# TXW8301-FMAC-SDK
TX_AH SDK 2.4 FMAC firmware development repository.

## Docker Build (TXW8301-14)

This repository includes a fully containerized FMAC firmware build for the TX_AH 2.4 SDK, using the vendor Windows CDK under Wine. The vendor baseline is tracked via git tags (e.g. `vendor-v2.4.1.5-42011`); the directory name is stable and version-independent.

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
- `docker/build-fmac-image.sh`: host-side wrapper (builds Docker image, bootstraps CDK)
- `build_fmac_firmware.sh`: top-level bus-select build wrapper — patches `project_config.h` in-memory and invokes Docker for the chosen interface (`-sdio`, `-usb`, `-uart`)

### Prerequisites

1. Docker on host (BuildKit-capable).
2. CDK installer extracted at (default):
   - `./cdk/cdk-windows-V2.8.8-20210621-1740/` (inside this FMAC repo)

If no CDK is present, the runner will auto-download the CDK into `./cdk/` by default to simplify first-time onboarding. To disable automatic download set `CDK_AUTO_FETCH=0`.

- `CDK_FTP_URL` set to Taixin FTP zip URL (default: `ftp://183.47.14.74/upload/cdk-windows-V2.8.8-20210621-1740.zip`)
- `CDK_FTP_USER` / `CDK_FTP_PASS`: optional FTP credentials (example: `txguest` / `txguest`)
- `CDK_SHA256` (recommended): `f3b19310c21bfb9597d9ff22f71284bbb880841355a370ba726783130f18993d`

Apple Silicon (M1/M2) note: On Apple Silicon (aarch64/arm64) hosts the runner will automatically build an amd64 image using Docker Buildx and QEMU so `wine`/`wine32` and the vendor i386 packages are available. Ensure Docker Desktop provides Buildx/QEMU support or run the one-time commands documented in `DOCKER_BUILD_NOTES.md` before building on M1/M2.

Note: the script includes a default FTP URL and an example credential pair for convenience. Do NOT commit private credentials into the repository; prefer passing credentials via environment variables or a secrets manager.

## Quickstart

- **Prereqs**: `Docker` (on Apple Silicon enable Buildx/QEMU) and `git`.
- **Bootstrap (example)**:

```bash
CDK_AUTO_FETCH=1 \
CDK_FTP_URL='ftp://183.47.14.74/upload/cdk-windows-V2.8.8-20210621-1740.zip' \
CDK_FTP_USER='txguest' \
CDK_FTP_PASS='txguest' \
CDK_SHA256='f3b19310c21bfb9597d9ff22f71284bbb880841355a370ba726783130f18993d' \
./docker/build-fmac-image.sh
```

- **Build (full)**:

```bash
./docker/build-fmac-image.sh
```

- **Compile-only**:

```bash
SKIP_PACKAGING=1 ./docker/build-fmac-image.sh
```

- **Apple Silicon note**: enable Docker Buildx and QEMU emulation; the runner will build an `linux/amd64` image on arm64 hosts (first run may be slower).

### Run

```bash
cd SDK/TX_AH_SDK_2.4/FMAC/TXW8301_FMAC
./docker/build-fmac-image.sh
```

Bus-interface build (recommended — selects SDIO, USB, or UART without modifying source):

```bash
./build_fmac_firmware.sh -sdio
./build_fmac_firmware.sh -usb
./build_fmac_firmware.sh -uart
```

Compile only (skip vendor packaging):

```bash
SKIP_PACKAGING=1 ./docker/build-fmac-image.sh
```

First-time bootstrap from FTP (example):

```bash
CDK_AUTO_FETCH=1 \
CDK_FTP_URL='ftp://183.47.14.74/upload/cdk-windows-V2.8.8-20210621-1740.zip' \
CDK_FTP_USER='txguest' \
CDK_FTP_PASS='txguest' \
CDK_SHA256='f3b19310c21bfb9597d9ff22f71284bbb880841355a370ba726783130f18993d' \
./docker/build-fmac-image.sh
```

Explicit path override:

```bash
CDK_DIR=/abs/path/to/cdk-windows-V2.8.8-20210621-1740 ./docker/build-fmac-image.sh
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

When using `build_fmac_firmware.sh`, the binary is renamed with a bus-mode suffix:

- `txw8301_vX.Y.Z-BBBBB_<date>_<mode>.bin` (e.g. `txw8301_v2.4.1.5-42011_2026.4.28_usb.bin`)

Generated artifacts are preserved in timestamped build folders; source/project files are left intact.

### Notes

- Vendor packaging batch scripts still print a Windows backup-path warning, but packaging succeeds.
- Generated outputs are ignored via repository `.gitignore`.

---

## CI / Release

The workflow file is at `.github/workflows/release.yml`. It builds the Docker image, runs the full firmware pipeline, and publishes a GitHub Release with `txw8301.bin` attached.

### Triggers

| Trigger | When |
|---|---|
| Push a `v*` tag | Automatically starts a release build |
| `workflow_dispatch` | Manual run from GitHub UI or CLI |

### Trigger via git tag (recommended for formal releases)

```bash
git tag -a TXW8301_FMAC-vX.Y.Z-BBBBB -m "Release TXW8301_FMAC-vX.Y.Z-BBBBB"
git push origin TXW8301_FMAC-vX.Y.Z-BBBBB
```

> Note: the tag-triggered path requires `./cdk/` to be present in the repo (CDK is not auto-fetched on tag push). If CDK is not committed, use the manual dispatch path below instead.

### Trigger manually (CDK auto-download via vendor FTP)

**GitHub CLI:**

```bash
# Minimal — uses all workflow defaults (FTP creds + firmware version tag)
gh workflow run release.yml -f cdk_auto_fetch=1

# Explicit — override the release tag
gh workflow run release.yml \
  -f cdk_auto_fetch=1 \
  -f release_tag=TXW8301_FMAC-vX.Y.Z-BBBBB
```

**GitHub web UI:**

Go to `Actions` → `Release Firmware` → `Run workflow` → set `cdk_auto_fetch` to `1` → click `Run workflow`.

### Watch live log output

```bash
# Stream live step status until run completes
gh run watch $(gh run list --limit 1 --json databaseId -q '.[0].databaseId')

# Or list runs first to pick a specific ID
gh run list --limit 5
gh run watch <RUN_ID>
```

### View full logs after a run

```bash
# Full log for all steps
gh run view <RUN_ID> --log

# Only failed steps
gh run view <RUN_ID> --log-failed
```

### Cancel or delete a run

```bash
# Cancel an in-progress or queued run
gh run cancel <RUN_ID>

# Delete the run record entirely (clean history)
gh run delete <RUN_ID>
```

### Workflow inputs reference

| Input | Default | Description |
|---|---|---|
| `cdk_auto_fetch` | `0` | Set to `1` to download CDK from vendor FTP |
| `cdk_ftp_url` | Taixin FTP URL | CDK zip download URL |
| `cdk_ftp_user` | `txguest` | FTP username (public vendor credential) |
| `cdk_ftp_pass` | `txguest` | FTP password (public vendor credential) |
| `cdk_sha256` | known hash | SHA256 of CDK zip for integrity verification |
| `release_tag` | `TXW8301_FMAC-vX.Y.Z-BBBBB` | Tag and name used for the GitHub Release (match the vendor build tag) |

Credentials can also be set as GitHub repository secrets (`CDK_FTP_USER`, `CDK_FTP_PASS`) to override the defaults without editing the workflow file.
