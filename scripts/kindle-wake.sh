#!/bin/sh
# Keep Amazon available until the next sleep/wake cycle after an explicit exit.
base=/mnt/us/kindle-lite
sleeping=no
[ "$(/usr/bin/lipc-get-prop com.lab126.powerd state 2>/dev/null)" = screenSaver ] && sleeping=yes
while :; do
    if event=$(/usr/bin/lipc-wait-event com.lab126.powerd goingToScreenSaver,outOfScreenSaver); then
        case "$event" in
            *goingToScreenSaver*) sleeping=yes; continue ;;
            *outOfScreenSaver*) [ "$sleeping" = yes ] || continue; sleeping=no ;;
            *) continue ;;
        esac
        [ -f "$base/autostart.enabled" ] || continue
        [ ! -f "$base/SAFE_MODE" ] || continue
        [ ! -d /tmp/kindle-lite-launch.lock ] || continue
        [ -x "$base/kindle_ui" ] && [ -f "$base/launch.sh" ] || continue
        mkdir -p "$base/runtime"
        KINDLE_LITE_RESTORE_AMAZON=yes sh "$base/launch.sh" >> "$base/runtime/wake.log" 2>&1 </dev/null &
    else
        sleep 1
    fi
done
