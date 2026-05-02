#!/bin/bash
set -e

echo "=== react-native-remarkable build ==="

DIST="./dist"
mkdir -p $DIST

echo "[1/3] Building C++ host (this takes ~10 min on first run)..."
docker build --platform linux/arm64 -t react-native-remarkable .

echo "[2/3] Copying binaries..."
docker run --rm --platform linux/arm64 \
  -v $(pwd)/dist:/out \
  react-native-remarkable \
  sh -c "cp /output/rn-layout /out/rn-layout && cp /output/libhermesvm.so /out/libhermesvm.so"

echo "[3/3] Bundling JS..."
cd template
npm install
npx react-native bundle \
  --platform android \
  --dev false \
  --entry-file index.js \
  --bundle-output ../dist/remarkable.bundle.js \
  --sourcemap-output ../dist/remarkable.bundle.js.map \
  --reset-cache
cd ..

echo ""
echo "Done! Output in ./dist:"
ls -lh $DIST
echo ""
echo "Deploy: ./scripts/deploy.sh <device-ip>"
