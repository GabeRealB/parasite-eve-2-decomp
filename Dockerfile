FROM ubuntu:25.10

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y -o APT::Immediate-Configure=false \
    build-essential \
    ninja-build \
    python3 \
    python3-pip \
    binutils-mipsel-linux-gnu \
    binutils-mips-linux-gnu \
    less make vim git wget curl file \
    && rm -rf /var/lib/apt/lists/*

RUN wget http://ftp.de.debian.org/debian/pool/main/g/gcc-14-cross-mipsen/gcc-14-mips-linux-gnu-base_14.2.0-13cross1_amd64.deb \
    && apt-get install ./gcc-14-mips-linux-gnu-base_14.2.0-13cross1_amd64.deb

RUN wget http://ftp.de.debian.org/debian/pool/main/g/gcc-14-cross-mipsen/cpp-14-mips-linux-gnu_14.2.0-13cross1_amd64.deb \
    && apt-get install ./cpp-14-mips-linux-gnu_14.2.0-13cross1_amd64.deb

RUN wget http://ftp.de.debian.org/debian/pool/main/g/gcc-defaults-mipsen/cpp-mips-linux-gnu_14.2.0-1_amd64.deb \
    && apt-get install ./cpp-mips-linux-gnu_14.2.0-1_amd64.deb

COPY rom/image /rom

WORKDIR /app
