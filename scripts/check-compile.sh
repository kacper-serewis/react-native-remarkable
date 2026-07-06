#!/bin/bash
# Fast compile check without the full Docker image: builds libquill.so for
# real and syntax-checks the host against Qt + Hermes/Yoga headers.
# Runs inside ubuntu:24.04 (see usage at bottom); much faster than the full
# QEMU Hermes build when only host/ changed.
set -euo pipefail

apt-get update -qq
apt-get install -y -qq g++ pkg-config qt6-base-dev git ca-certificates > /dev/null

QTFLAGS="$(pkg-config --cflags Qt6Gui Qt6Network Qt6Core) -fPIC -std=c++17"
QTLIBS="$(pkg-config --libs Qt6Gui Qt6Core)"
QUILL=/src/riddle/quill/src

echo "== stub libqsgepaper.so =="
g++ $QTFLAGS -shared -I$QUILL /src/host/qsgepaper_stub.cpp \
    -Wl,-soname,libqsgepaper.so -o /tmp/libqsgepaper.so $QTLIBS

echo "== libquill.so (epfb shim + host C ABI) =="
g++ $QTFLAGS -shared -I$QUILL $QUILL/epfb.cpp /src/host/quill_host.cpp \
    -L/tmp -l:libqsgepaper.so -o /tmp/libquill.so $QTLIBS -ldl

echo "== main.cpp syntax check =="
if [ ! -d /tmp/hermes ]; then
  git clone -q --depth 1 https://github.com/facebook/hermes /tmp/hermes
fi
if [ ! -d /tmp/yoga ]; then
  git clone -q --depth 1 https://github.com/facebook/yoga /tmp/yoga
fi
g++ $QTFLAGS -fsyntax-only \
    -I/tmp/hermes/API -I/tmp/hermes/API/jsi -I/tmp/hermes/public -I/tmp/hermes/include \
    -I/tmp/yoga -I/src/host \
    /src/host/main.cpp

echo "== symbols exported by libquill.so =="
nm -D --defined-only /tmp/libquill.so | grep -E "quill_|QImage" || true
echo "== DT_NEEDED of libquill.so =="
readelf -d /tmp/libquill.so | grep NEEDED

echo "OK"

# Usage:
#   docker run --rm -v "$(pwd)":/src ubuntu:24.04 bash /src/scripts/check-compile.sh
