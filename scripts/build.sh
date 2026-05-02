#!/bin/bash
set -e

echo "=== react-native-remarkable build script ==="

# ── Config ────────────────────────────────────────────────────────
CONTAINER="hermes-build3"
OUT="./dist"
mkdir -p $OUT

echo "[1/3] Building C++ host in Docker..."
docker start $CONTAINER 2>/dev/null || true
docker exec $CONTAINER bash -c "
  cd /rn_layout && cmake --build build
"
docker cp $CONTAINER:/rn_layout/build/rn-layout $OUT/rn-layout
docker cp $CONTAINER:/hermes/build/lib/libhermesvm.so $OUT/libhermesvm.so

echo "[2/3] Building JS bundle..."
cd template
npx react-native bundle \
  --platform android \
  --dev false \
  --entry-file index.js \
  --bundle-output ../dist/remarkable.bundle.js \
  --reset-cache
cd ..

echo "[3/3] Done! Output in ./dist:"
ls -lh $OUT
echo ""
echo "Deploy with: ./scripts/deploy.sh <device-ip>"
