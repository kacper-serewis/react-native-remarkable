#!/bin/bash
set -e

DEVICE=${1:-192.168.1.196}
ROOT="$(dirname $0)/.."

echo "=== Rebuilding JS + deploying ==="

cd $ROOT/template
npx react-native bundle \
  --platform android \
  --dev false \
  --entry-file index.js \
  --bundle-output ../dist/remarkable.bundle.js \
  --sourcemap-output ../dist/remarkable.bundle.js.map \
  --reset-cache
cd ..

scp dist/remarkable.bundle.js root@$DEVICE:~/rn-app/remarkable.bundle.js

ssh root@$DEVICE "
  systemctl stop xochitl 2>/dev/null || true
  kill \$(ps | grep rn-layout | grep -v grep | awk '{print \$1}') 2>/dev/null || true
  LD_LIBRARY_PATH=~/hermes-host \
  ~/rn-app/rn-layout ~/rn-app/remarkable.bundle.js
" 2>&1 | node "$ROOT/scripts/translate-stack.js"
