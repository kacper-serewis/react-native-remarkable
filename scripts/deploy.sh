#!/bin/bash
set -e

DEVICE=${1:-192.168.1.196}
SCRIPTS="$(dirname $0)"
DIST="$SCRIPTS/../dist"

if [ ! -f "$DIST/rn-layout" ]; then
  echo "Error: dist/ not found. Run ./scripts/build.sh first."
  exit 1
fi

echo "=== Deploying to $DEVICE ==="

echo "[1/3] Copying binaries..."
ssh root@$DEVICE "
  mkdir -p ~/hermes-host ~/rn-app
  kill \$(ps | grep rn-layout | grep -v grep | awk '{print \$1}') 2>/dev/null || true
  rm -f ~/rn-app/rn-layout ~/hermes-host/libhermesvm.so
"
scp $DIST/rn-layout root@$DEVICE:~/rn-app/rn-layout
scp $DIST/libhermesvm.so root@$DEVICE:~/hermes-host/libhermesvm.so
ssh root@$DEVICE "chmod +x ~/rn-app/rn-layout"

echo "[2/3] Copying bundle..."
scp $DIST/remarkable.bundle.js root@$DEVICE:~/rn-app/remarkable.bundle.js

echo "[3/3] Launching..."
ssh root@$DEVICE "
  kill \$(ps | grep xochitl | grep -v grep | awk '{print \$1}') 2>/dev/null || true
  kill \$(ps | grep rn-layout | grep -v grep | awk '{print \$1}') 2>/dev/null || true
  LD_LIBRARY_PATH=~/hermes-host \
  QT_QUICK_BACKEND=epaper \
  ~/rn-app/rn-layout ~/rn-app/remarkable.bundle.js -platform epaper
" 2>&1 | node "$SCRIPTS/translate-stack.js"
