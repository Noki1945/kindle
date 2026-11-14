#!/bin/sh
set -eu
base=/mnt/us/kindle-lite
[ -x "$base/kindle_ui" ] && [ -f "$base/boot.sh" ] && [ -f "$base/power-bridge.lua" ]
[ -f "$base/wake.sh" ] && [ -f "$base/kindle-lite-wake.conf" ]
"$base/kindle_ui" --probe
if [ -f /etc/upstart/kindle-lite.conf ]; then
    mkdir -p "$base/runtime/install-backup"
    cp /etc/upstart/kindle-lite.conf "$base/runtime/install-backup/kindle-lite.conf.$(date +%Y%m%d%H%M%S)"
fi
trap 'mntroot ro' EXIT
mntroot rw
cp "$base/kindle-lite.conf" /etc/upstart/kindle-lite.conf
chmod 0644 /etc/upstart/kindle-lite.conf
cp "$base/kindle-lite-wake.conf" /etc/upstart/kindle-lite-wake.conf
chmod 0644 /etc/upstart/kindle-lite-wake.conf
for job in lab126_gui otaupd otav3; do
    source="/etc/upstart/$job.conf"
    backup="$base/runtime/install-backup/$job.conf.before-direct-boot"
    mkdir -p "$base/runtime/install-backup"
    [ -f "$backup" ] || cp "$source" "$backup"
    # Kindle's Upstart 0.6.6 predates .override support. Keep the original job
    # intact except for its single automatic start stanza; explicit start works.
    sed 's/^start on /# Kindle Lite disabled: start on /' "$source" > "$source.kindle-lite-tmp"
    chmod 0644 "$source.kindle-lite-tmp"
    mv "$source.kindle-lite-tmp" "$source"
    override="/etc/upstart/$job.override"
    if [ -f "$override" ] && grep -q '^# Kindle Lite managed override' "$override"; then
        rm -f "$override"
    fi
done
sync
mntroot ro
trap - EXIT
initctl reload-configuration || true
# Some firmware reloads time out on D-Bus even though the new job was loaded.
# Require the specific job to exist before enabling its trigger file.
status kindle-lite
touch "$base/autostart.enabled"
status kindle-lite-wake | grep -q start/running || start kindle-lite-wake
echo 'Autostart enabled; SAFE_MODE in the Kindle Lite directory bypasses it.'
