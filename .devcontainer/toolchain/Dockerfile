ARG VARIANT=ubuntu-22.04
FROM mcr.microsoft.com/vscode/devcontainers/cpp:0-${VARIANT}

RUN apt-get update && \
    export DEBIAN_FRONTEND=noninteractive && \
    apt-get -y install --no-install-recommends \
    autoconf \
    automake \
    bc \
    bison \
    cpio \
    device-tree-compiler \
    file \
    flex \
    libfl-dev \
    libglib2.0-dev \
    libssl-dev \
    libubootenv-tool \
    libyaml-dev \
    mercurial \
    python3-dev \
    rsync \
    swig \
    u-boot-tools \
    wget

COPY scripts/* /usr/local/bin/
RUN chmod +x /usr/local/bin/*