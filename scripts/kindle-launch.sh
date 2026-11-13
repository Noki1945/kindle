#!/bin/sh
# The stock GUI is restored on exit or renderer failure.
cd /mnt/us/kindle-lite || exit 1
if ! mkdir /tmp/kindle-lite-launch.lock 2>/dev/null; then exit 1; fi
restore_gui() {
    trap - EXIT INT TERM HUP
    if [ -n "$UI_PID" ]; then
        kill -TERM "$UI_PID" 2>/dev/null
        wait "$UI_PID" 2>/dev/null
    fi
    if [ -n "$BRIDGE_PID" ]; then
        kill -TERM "$BRIDGE_PID" 2>/dev/null
        wait "$BRIDGE_PID" 2>/dev/null
    fi
    if [ "$GUI_WAS_RUNNING" = yes ] && status x | grep -q start/running; then start lab126_gui; fi
    rmdir /tmp/kindle-lite-launch.lock
}
stop_session() { exit 143; }
GUI_WAS_RUNNING=no
UI_PID=
BRIDGE_PID=
if status lab126_gui | grep -q 'start/running'; then GUI_WAS_RUNNING=yes; fi
trap restore_gui EXIT
trap stop_session INT TERM HUP
if [ "$(lipc-get-prop com.lab126.powerd state)" != active ]; then
    lipc-set-prop -i com.lab126.powerd wakeUp 1 || exit 1
fi
if [ "$GUI_WAS_RUNNING" = yes ]; then stop lab126_gui || exit 1; fi
mkdir -p runtime
/usr/bin/lua ./power-bridge.lua > runtime/power-bridge.log 2>&1 &
BRIDGE_PID=$!
bridge_ready=no
for attempt in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30; do
    if [ "$(lipc-get-prop -i com.lab126.kaf frameworkStarted 2>/dev/null)" = 1 ]; then bridge_ready=yes; break; fi
    kill -0 "$BRIDGE_PID" 2>/dev/null || break
    usleep 200000
done
[ "$bridge_ready" = yes ] || exit 1
./kindle_ui "$@" &
UI_PID=$!
echo "SESSION ui=$UI_PID launcher=$$"
wait "$UI_PID"
result=$?
UI_PID=
exit "$result"
