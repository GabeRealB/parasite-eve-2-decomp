FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y -o APT::Immediate-Configure=false \
    build-essential \
    ninja-build \
    python3 \
    python3-pip \
    binutils-mipsel-linux-gnu \
    binutils-mips-linux-gnu \
    cpp-mips-linux-gnu \
    less make vim git wget curl file \
    && rm -rf /var/lib/apt/lists/*

COPY rom/image /rom

WORKDIR /app
