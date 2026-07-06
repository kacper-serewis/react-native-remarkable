#!/bin/sh
# Runs on the reMarkable Paper Pro. Stops xochitl so we own the panel,
# then launches the app. The quill backend takes over the vendor e-ink
# engine directly — no window system, no Qt Quick.
#
# Exit the app with a 5-finger tap or the power button.

set -e

# Stop the default UI. systemctl (not kill) so systemd doesn't respawn it.
systemctl stop xochitl 2>/dev/null || true
kill $(ps | grep rn-layout | grep -v grep | awk '{print $1}') 2>/dev/null || true

LD_LIBRARY_PATH="$HOME/hermes-host" \
exec "$HOME/rn-app/rn-layout" "$HOME/rn-app/remarkable.bundle.js"
