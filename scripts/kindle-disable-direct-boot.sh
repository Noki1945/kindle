#!/bin/sh
set -eu
base=/mnt/us/kindle-lite
rm -f "$base/autostart.enabled"
trap 'mntroot ro' EXIT
mntroot rw
override=/etc/upstart/lab126_gui.override
backup="$base/runtime/install-backup/lab126_gui.conf.before-direct-boot"
if [ -f "$backup" ]; then cp "$backup" /etc/upstart/lab126_gui.conf; chmod 0644 /etc/upstart/lab126_gui.conf; fi
if [ -f "$override" ] && grep -q '^# Kindle Lite managed override' "$override"; then
    if [ -f "$base/runtime/install-backup/lab126_gui.override.before-direct-boot" ]; then
        cp "$base/runtime/install-backup/lab126_gui.override.before-direct-boot" "$override"
    else
        rm -f "$override"
    fi
fi
mntroot ro
trap - EXIT
initctl reload-configuration || true
echo 'Direct boot disabled; firmware auto-updates remain disabled.'
