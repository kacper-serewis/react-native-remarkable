#!/bin/bash
set -e

DEVICE=${1:-192.168.1.196}
DIST="./dist"

echo "=== Deploying to $DEVICE ==="

echo "[1/3] Copying binaries..."
ssh root@$DEVICE "mkdir -p ~/hermes-host ~/rn-app"
scp $DIST/rn-layout root@$DEVICE:~/rn-app/rn-layout
scp $DIST/libhermesvm.so root@$DEVICE:~/hermes-host/libhermesvm.so

echo "[2/3] Copying bundle..."
scp $DIST/remarkable.bundle.js root@$DEVICE:~/rn-app/remarkable.bundle.js

echo "[3/3] Launching app..."
ssh root@$DEVICE "
  kill \$(ps | grep xochitl | grep -v grep | awk '{print \$1}') 2>/dev/null || true
  kill \$(ps | grep rn-layout | grep -v grep | awk '{print \$1}') 2>/dev/null || true
  chmod +x ~/rn-app/rn-layout
  LD_LIBRARY_PATH=~/hermes-host \
  QT_QUICK_BACKEND=epaper \
  ~/rn-app/rn-layout ~/rn-app/remarkable.bundle.js -platform epaper
"
