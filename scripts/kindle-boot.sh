#!/bin/sh
# Run once per boot. A crash or return to Amazon must never form a restart loop.
base=/mnt/us/kindle-lite
fallback() { echo "FALLBACK ${1:-unknown} $(cat /proc/uptime)"; status lab126_gui | grep -q start/running || start lab126_gui; exit 0; }
exec >> /var/local/kindle-lite-startup.log 2>&1
echo "BOOT_START $(cat /proc/uptime)"
if [ -f "$base/ensure-usb-ssh.sh" ]; then
    mkdir -p "$base/runtime"
    sh "$base/ensure-usb-ssh.sh" >> "$base/runtime/usb-ssh.log" 2>&1 </dev/null &
fi
[ -f "$base/autostart.enabled" ] || fallback disabled
[ ! -f "$base/SAFE_MODE" ] || fallback safe-mode
[ -x "$base/kindle_ui" ] && [ -f "$base/launch.sh" ] || fallback missing-files
mkdir /tmp/kindle-lite-boot-attempted 2>/dev/null || exit 0
# Wait for the existing low-level display and power services, without starting
# Amazon's application framework or imposing a fixed fifteen-second delay.
ready=no
for attempt in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20; do
    if lipc-get-prop com.lab126.blanket load >/dev/null 2>&1 && lipc-get-prop com.lab126.powerd state >/dev/null 2>&1; then ready=yes; break; fi
    usleep 250000
done
[ "$ready" = yes ] || fallback services-not-ready
[ ! -f "$base/SAFE_MODE" ] || fallback safe-mode
mkdir -p "$base/runtime"
cd "$base" || exit 1
./kindle_ui --probe >> runtime/boot.log 2>&1 || fallback probe-failed
export KINDLE_LITE_RESTORE_AMAZON=yes
echo "UI_LAUNCH $(cat /proc/uptime)"
exec sh ./launch.sh >> runtime/boot.log 2>&1
