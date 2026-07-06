FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

# Note: qt6-declarative-dev/libqt6quick6 are no longer needed (Qt Quick was
# replaced by quill) but stay listed so the Hermes/Yoga layer cache survives.
RUN apt-get update && apt-get install -y \
  git cmake ninja-build python3 python3-pip \
  build-essential patchelf pkg-config \
  libicu-dev libdrm-dev libcairo2-dev \
  qt6-base-dev qt6-declarative-dev libqt6quick6 \
  wget curl xz-utils \
  && rm -rf /var/lib/apt/lists/*

# ── Build Hermes ──────────────────────────────────────────────────
RUN git clone https://github.com/facebook/hermes.git /hermes && \
  cd /hermes && git checkout main

RUN cmake -S /hermes -B /hermes/build \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DHERMES_BUILD_SHARED_JSI=OFF \
  -DHERMES_ENABLE_INTL=OFF \
  -DHERMES_ENABLE_DEBUGGER=OFF && \
  cmake --build /hermes/build --target hermes hermesvm jsi -j$(nproc)

# ── Build Yoga ────────────────────────────────────────────────────
RUN git clone https://github.com/facebook/yoga.git /yoga && \
  cmake -S /yoga -B /yoga/build \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=OFF && \
  cmake --build /yoga/build -j$(nproc)

# ── Build host ────────────────────────────────────────────────────
# quill: e-ink display shim sourced from the riddle submodule.
COPY riddle/quill/src /quill/src
COPY host/ /rn_host/

RUN cmake -S /rn_host -B /rn_host/build \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DQUILL_SRC=/quill/src && \
  cmake --build /rn_host/build

# ── Collect output ────────────────────────────────────────────────
RUN mkdir -p /output && \
  cp /rn_host/build/rn-layout /output/rn-layout && \
  cp /rn_host/build/libquill.so /output/libquill.so && \
  cp /hermes/build/lib/libhermesvm.so /output/libhermesvm.so && \
  patchelf --set-rpath '$ORIGIN' /output/rn-layout && \
  patchelf --set-rpath '$ORIGIN:/usr/lib/plugins/scenegraph' /output/libquill.so

WORKDIR /output
