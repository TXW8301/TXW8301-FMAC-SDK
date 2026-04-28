# syntax=docker/dockerfile:1
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=UTC
ENV WINEPREFIX=/opt/cdk-wineprefix
ENV WINEARCH=win32
ENV WINEDEBUG=-all

# Enable i386 architecture for wine32
RUN dpkg --add-architecture i386 \
    && for i in 1 2 3 4 5; do apt-get update && break || sleep 5; done \
    && apt-get install -y --no-install-recommends \
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
# wineboot/wineserver leave socket dirs in /tmp owned by root; remove them
# so they are not baked into the image layer (they would block the builder
# user from creating its own socket dir at container start time).
RUN mkdir -p "${WINEPREFIX}" \
    && wineboot --init 2>/dev/null || true \
    && wineserver --wait || true \
    && rm -rf /tmp/.wine-* /tmp/wine-*

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

# ---------------------------------------------------------------------------
# Non-root build user
# BUILD_UID / BUILD_GID are passed at image-build time by build-fmac-image.sh
# to match the host developer's UID/GID.  The wineprefix must be owned by
# the user that runs Wine, so we chown it here.  At container run time
# --user $(id -u):$(id -g) satisfies Wine's ownership check with no sudo.
# ---------------------------------------------------------------------------
ARG BUILD_UID=1000
ARG BUILD_GID=1000
RUN groupadd --gid "${BUILD_GID}" builder 2>/dev/null || true \
    && useradd --uid "${BUILD_UID}" --gid "${BUILD_GID}" \
       --no-create-home --shell /bin/bash builder 2>/dev/null || true \
    && chown -R "${BUILD_UID}:${BUILD_GID}" "${WINEPREFIX}"

USER builder

ENTRYPOINT ["/usr/local/bin/container-build.sh"]
