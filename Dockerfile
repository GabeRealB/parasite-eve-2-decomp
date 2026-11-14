FROM ubuntu:25.10

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y -o APT::Immediate-Configure=false \
    build-essential \
    ninja-build \
    python3 \
    python3-pip \
    # objdump, for the diff tools: the build fetches its toolchain (tools/config/fetch_tools.py).
    binutils-mips-linux-gnu \
    less make vim git wget curl file \
    && rm -rf /var/lib/apt/lists/*

COPY rom/image /rom

WORKDIR /app
