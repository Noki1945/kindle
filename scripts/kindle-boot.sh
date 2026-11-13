#!/bin/sh
# Run once per boot. A crash or return to Amazon must never form a restart loop.
base=/mnt/us/kindle-lite
[ -f "$base/autostart.enabled" ] || exit 0
[ ! -f "$base/SAFE_MODE" ] || exit 0
[ -x "$base/kindle_ui" ] && [ -f "$base/launch.sh" ] || exit 0
mkdir /tmp/kindle-lite-boot-attempted 2>/dev/null || exit 0
sleep 15
[ ! -f "$base/SAFE_MODE" ] || exit 0
status lab126_gui | grep -q start/running || exit 0
mkdir -p "$base/runtime"
cd "$base" || exit 1
./kindle_ui --probe >> runtime/boot.log 2>&1 || exit 1
exec sh ./launch.sh >> runtime/boot.log 2>&1
