#!/bin/sh
# Runs on the reMarkable Paper Pro. Stops xochitl so we own the screen,
# then launches the app under the epaper QPA platform plugin.

set -e

# Stop the default UI if it's running
kill $(ps | grep -E 'xochitl|rn-layout' | grep -v grep | awk '{print $1}') 2>/dev/null || true

LD_LIBRARY_PATH="$HOME/hermes-host" \
QT_QUICK_BACKEND=epaper \
exec "$HOME/rn-app/rn-layout" "$HOME/rn-app/remarkable.bundle.js" -platform epaper
