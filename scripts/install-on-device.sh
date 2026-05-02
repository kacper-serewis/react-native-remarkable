#!/bin/sh
# Runs on a reMarkable Paper Pro. Places binaries and bundle into
# ~/rn-app and ~/hermes-host, then prints how to launch the app.

set -e
HERE="$(cd "$(dirname "$0")" && pwd)"

echo "=== Installing react-native-remarkable ==="

mkdir -p "$HOME/rn-app" "$HOME/hermes-host"

cp "$HERE/rn-layout"             "$HOME/rn-app/rn-layout"
cp "$HERE/libhermesvm.so"        "$HOME/hermes-host/libhermesvm.so"
cp "$HERE/remarkable.bundle.js"  "$HOME/rn-app/remarkable.bundle.js"
cp "$HERE/start.sh"              "$HOME/rn-app/start.sh"

chmod +x "$HOME/rn-app/rn-layout" "$HOME/rn-app/start.sh"

cat <<'EOF'

Installed.

Launch with:    ~/rn-app/start.sh

Note: start.sh stops xochitl (the default reMarkable UI) to release
the screen. To restore xochitl, reboot the device or run:
    systemctl start xochitl
EOF
