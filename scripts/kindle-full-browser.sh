#!/bin/sh
set -u
base=/mnt/us/kindle-lite
engine="$base/engines/browser/kbrowser.sh"
url="${1:-https://www.google.com/}"
case "$url" in https://*) ;; *) echo 'Only HTTPS addresses are accepted'; exit 1 ;; esac
[ -f "$engine" ] && [ -f /mnt/us/kbrowser/kbrowser.ext3 ] || exit 1
child=
vpn_session=no
pillow_was_running=no
status pillow | grep -q start/running && pillow_was_running=yes
cleanup() {
    trap - EXIT INT TERM HUP
    if [ -n "$child" ]; then kill -TERM "$child" 2>/dev/null; wait "$child" 2>/dev/null; fi
    if grep -q ' /var/tmp/kbrowser ' /proc/mounts; then sh "$engine" cleanup; fi
    if [ "$vpn_session" = yes ]; then sh "$base/vpn.sh" stop || true; fi
    # The old mount utility can leave loop devices attached after unmounting.
    for backing in /sys/block/loop*/loop/backing_file; do
        read -r image < "$backing" || continue
        case "$image" in
            /mnt/us/kbrowser/kbrowser.ext3|/mnt/base-us/kbrowser/kbrowser.ext3) ;;
            *) continue ;;
        esac
        loop_name=${backing#/sys/block/}
        loop_name=${loop_name%%/*}
        device=/dev/loop/${loop_name#loop}
        if ! awk -v device="$device" -v alias="/dev/$loop_name" '$1 == device || $1 == alias { found = 1 } END { exit !found }' /proc/mounts; then
            losetup -d "$device" 2>/dev/null || true
        fi
    done
    lipc-set-prop -i com.lab126.powerd preventScreenSaver 0 2>/dev/null || true
    if [ "$pillow_was_running" = no ]; then stop pillow 2>/dev/null || true; fi
}
trap cleanup EXIT
trap 'exit 143' INT TERM HUP
if [ -f "$base/vpn.enabled" ]; then
    [ -f "$base/vpn.sh" ] && sh "$base/vpn.sh" start || exit 1
    vpn_session=yes
fi
if [ "$pillow_was_running" = no ]; then start pillow || exit 1; fi
export KINDLE_LITE_URL="$url"
sh "$engine" start chromium &
child=$!
wait "$child"
result=$?
child=
exit "$result"
