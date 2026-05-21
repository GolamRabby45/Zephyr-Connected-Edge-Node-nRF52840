FROM ubuntu:22.04

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    git cmake ninja-build gperf ccache dfu-util device-tree-compiler wget \
    python3 python3-pip python3-venv python3-setuptools python3-wheel \
    xz-utils file make gcc gcc-multilib g++-multilib libsdl2-dev \
    libmagic1 && rm -rf /var/lib/apt/lists/*

RUN pip3 install --no-cache-dir west

WORKDIR /workspace
CMD ["/bin/bash"]
