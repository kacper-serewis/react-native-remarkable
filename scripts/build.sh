#!/bin/bash
set -e

echo "=== react-native-remarkable build ==="

DIST="./dist"
mkdir -p $DIST

# The quill display backend needs the epfb shim sources from the submodule.
if [ ! -f riddle/quill/src/epfb.cpp ]; then
  echo "[0/3] Fetching riddle submodule (quill display backend)..."
  git submodule update --init riddle
fi

echo "[1/3] Building C++ host (this takes ~10 min on first run)..."
docker build --platform linux/arm64 -t react-native-remarkable .

echo "[2/3] Copying binaries..."
docker run --rm --platform linux/arm64 \
  -v $(pwd)/dist:/out \
  react-native-remarkable \
  sh -c "cp /output/rn-layout /out/rn-layout && cp /output/libquill.so /out/libquill.so && cp /output/libhermesvm.so /out/libhermesvm.so"

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
