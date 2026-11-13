#!/bin/sh
set -eu
base=/mnt/us/kindle-lite
[ -x "$base/kindle_ui" ] && [ -f "$base/boot.sh" ] && [ -f "$base/power-bridge.lua" ]
"$base/kindle_ui" --probe
if [ -f /etc/upstart/kindle-lite.conf ]; then
    mkdir -p "$base/runtime/install-backup"
    cp /etc/upstart/kindle-lite.conf "$base/runtime/install-backup/kindle-lite.conf.$(date +%Y%m%d%H%M%S)"
fi
trap 'mntroot ro' EXIT
mntroot rw
cp "$base/kindle-lite.conf" /etc/upstart/kindle-lite.conf
chmod 0644 /etc/upstart/kindle-lite.conf
sync
mntroot ro
trap - EXIT
initctl reload-configuration || true
# Some firmware reloads time out on D-Bus even though the new job was loaded.
# Require the specific job to exist before enabling its trigger file.
status kindle-lite
touch "$base/autostart.enabled"
echo 'Autostart enabled; SAFE_MODE in the Kindle Lite directory bypasses it.'
