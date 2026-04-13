FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=UTC

RUN apt-get update && apt-get install -y --no-install-recommends \
    bash \
    ca-certificates \
    file \
    make \
    python3 \
    python3-pip \
    p7zip-full \
    unzip \
    xz-utils \
    wine64 \
    winbind \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /work

COPY docker/scripts/container-build.sh /usr/local/bin/container-build.sh
RUN chmod +x /usr/local/bin/container-build.sh

ENTRYPOINT ["/usr/local/bin/container-build.sh"]
