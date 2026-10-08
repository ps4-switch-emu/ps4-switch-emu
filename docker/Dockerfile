# Build environment for Eden on PS4.
#
# x86_64 Ubuntu (runs under Rosetta on Apple Silicon): PacBrew's OpenOrbis tools (create-fself,
# PkgTool.Core, ld.lld 12) are only published for linux-x64.
#
# Toolchain, following DolphinPS4 (vendor/DolphinPS4/BUILDING.md): clang 21 + libc++ 21 built for
# the PS4 on top of the OpenOrbis C runtime from PacBrew. libc++ and Mesa are built later into
# the /build volume by scripts/, not baked into the image.

FROM --platform=linux/amd64 ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive
ARG CLANG_VER=21

RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates curl wget gnupg lsb-release software-properties-common \
        build-essential git ninja-build pkg-config nasm autoconf automake libtool \
        python3 python3-pip python3-venv bison flex xz-utils zip unzip file \
        libarchive-tools pacman-package-manager makepkg \
        glslang-tools libxml2 libicu74 \
    && rm -rf /var/lib/apt/lists/*

# clang/lld/llvm 21 from apt.llvm.org.
RUN curl -fsSL https://apt.llvm.org/llvm-snapshot.gpg.key | gpg --dearmor -o /usr/share/keyrings/llvm.gpg \
    && echo "deb [signed-by=/usr/share/keyrings/llvm.gpg] http://apt.llvm.org/noble/ llvm-toolchain-noble-${CLANG_VER} main" \
        > /etc/apt/sources.list.d/llvm.list \
    && apt-get update && apt-get install -y --no-install-recommends \
        clang-${CLANG_VER} lld-${CLANG_VER} llvm-${CLANG_VER} \
    && rm -rf /var/lib/apt/lists/*

# Eden needs CMake >= 3.31; Mesa 26 needs a recent meson. Kept in a venv on PATH.
RUN python3 -m venv /opt/pytools \
    && /opt/pytools/bin/pip install --no-cache-dir "cmake>=3.31,<4" meson mako pyyaml pillow packaging
ENV PATH="/opt/pytools/bin:${PATH}"

# PacBrew OpenOrbis toolchain -> /opt/pacbrew/ps4/openorbis.
RUN printf '\n[pacbrew]\nSigLevel = Optional TrustAll\nServer = https://pacman.mydedibox.fr/pacbrew/packages/\n' \
        >> /etc/pacman.conf \
    && pacman -Sy --noconfirm \
    && pacman -S --noconfirm ps4-openorbis

# PS4 port libraries Mesa's configure needs (zlib).
RUN pacman -Sy --noconfirm && pacman -S --noconfirm ps4-openorbis-zlib

# SVG -> PNG for the app icon.
RUN apt-get update && apt-get install -y --no-install-recommends librsvg2-bin && rm -rf /var/lib/apt/lists/*

# Current OpenOrbis create-fself built from source: PacBrew's (Nov 2024) predates the fix for
# duplicated data segments (OpenOrbis/create-fself 626d440), which the console rejects with
# CE-41839-5. Installed next to PacBrew's as create-fself-git.
RUN apt-get update && apt-get install -y --no-install-recommends golang-go && rm -rf /var/lib/apt/lists/* \
    && git clone --depth 1 https://github.com/OpenOrbis/create-fself /tmp/create-fself \
    && cd /tmp/create-fself/cmd/create-fself && cp go-linux.mod go.mod \
    && go build -o /opt/pacbrew/ps4/openorbis/bin/create-fself-git . \
    && git -C /tmp/create-fself log -1 --format='create-fself %h %cd' > /opt/pacbrew/ps4/openorbis/bin/create-fself-git.version \
    && rm -rf /tmp/create-fself /root/go /root/.cache/go-build

# Headers for the native Linux build of Eden (PC baseline for benchmarks).
RUN apt-get update && apt-get install -y --no-install-recommends libx11-dev libxext-dev libxrandr-dev \
        libwayland-dev libxkbcommon-dev libva-dev libdrm-dev && rm -rf /var/lib/apt/lists/*

ENV OPENORBIS=/opt/pacbrew/ps4/openorbis \
    OO_PS4_TOOLCHAIN=/opt/pacbrew/ps4/openorbis \
    DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1 \
    PSB=/build \
    PS4_BUILD_ROOT=/build \
    PS4_SYSROOT=/build/sysroot

WORKDIR /work
