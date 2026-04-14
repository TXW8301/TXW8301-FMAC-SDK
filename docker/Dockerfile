# syntax=docker/dockerfile:1
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=UTC
ENV WINEPREFIX=/opt/cdk-wineprefix
ENV WINEARCH=win32
ENV WINEDEBUG=-all

# Enable i386 architecture for wine32
RUN dpkg --add-architecture i386 \
    && apt-get update && apt-get install -y --no-install-recommends \
        bash \
        ca-certificates \
        file \
        make \
        python3 \
        p7zip-full \
        unzip \
        xz-utils \
        wine \
        wine32 \
        winbind \
        unshield \
        xvfb \
    && rm -rf /var/lib/apt/lists/*

# Initialise the 32-bit Wine prefix (required before any wine command)
RUN mkdir -p "${WINEPREFIX}" \
    && wineboot --init 2>/dev/null || true \
    && wineserver --wait || true

# ---------------------------------------------------------------------------
# CDK toolchain installation
# The CDK installer directory is supplied as a named build context:
#   docker build --build-context cdk-installer=<path/to/cdk-installer-dir>
# ---------------------------------------------------------------------------
COPY --from=cdk-installer . /tmp/cdk-setup/

COPY docker/scripts/install-cdk.sh /usr/local/bin/install-cdk.sh
RUN chmod +x /usr/local/bin/install-cdk.sh \
    && /usr/local/bin/install-cdk.sh \
    && rm -rf /tmp/cdk-setup

# ---------------------------------------------------------------------------
# Runtime build tooling
# ---------------------------------------------------------------------------
COPY docker/scripts/cdkproj_to_makefile.py /usr/local/bin/cdkproj_to_makefile.py
RUN chmod +x /usr/local/bin/cdkproj_to_makefile.py

WORKDIR /work

COPY docker/scripts/container-build.sh /usr/local/bin/container-build.sh
RUN chmod +x /usr/local/bin/container-build.sh

ENTRYPOINT ["/usr/local/bin/container-build.sh"]
