#!/bin/sh
# The private profile/core are provisioned separately and never packaged.
set -eu
base=/mnt/us/kindle-lite
private=$base/private/vpn
pidfile=$private/pid
running() {
    [ -f "$pidfile" ] || return 1
    read -r pid < "$pidfile"
    case "$pid" in ''|*[!0-9]*) return 1 ;; esac
    [ "$(readlink /proc/"$pid"/exe 2>/dev/null)" = "$private/xray" ] && kill -0 "$pid" 2>/dev/null
}
case "${1:-status}" in
    status) if running; then echo running; else echo stopped; exit 1; fi; exit 0 ;;
    start|stop) ;;
    *) echo 'usage: vpn.sh start|stop|status' >&2; exit 2 ;;
esac
lock=/tmp/kindle-lite-vpn.lock
mkdir "$lock" 2>/dev/null || { echo 'VPN operation already in progress' >&2; exit 1; }
trap 'rmdir "$lock"' EXIT
trap 'exit 143' INT TERM HUP
if [ "$1" = stop ]; then
    if running; then
        kill -TERM "$pid"
        attempt=0
        while running; do
            attempt=$((attempt + 1))
            [ "$attempt" -lt 25 ] || { echo 'VPN stop timed out' >&2; exit 1; }
            usleep 200000
        done
    fi
    rm -f "$pidfile"
    exit 0
fi
if running; then exit 0; fi
if grep -q '0100007F:2A38 .* 0A ' /proc/net/tcp; then
    echo 'VPN proxy port is already in use by another process' >&2; exit 1
fi
[ -x "$private/xray" ] && [ -f "$private/config.json" ] || { echo 'Private VPN installation missing' >&2; exit 1; }
umask 077
if ! "$private/xray" run -test -config "$private/config.json" >"$private/validation.log" 2>&1; then
    echo 'VPN profile rejected; details kept in private/validation.log' >&2; exit 1
fi
nohup "$private/xray" run -config "$private/config.json" >"$private/runtime.log" 2>&1 </dev/null &
pid=$!
printf '%s\n' "$pid" > "$pidfile"
attempt=0
while ! grep -q '0100007F:2A38 .* 0A ' /proc/net/tcp; do
    # The freshly forked child may still be inside nohup before exec(xray).
    if ! kill -0 "$pid" 2>/dev/null; then echo 'VPN process failed' >&2; rm -f "$pidfile"; exit 1; fi
    attempt=$((attempt + 1))
    if [ "$attempt" -ge 15 ]; then
        kill -TERM "$pid" 2>/dev/null || true
        rm -f "$pidfile"
        echo 'VPN listener timed out' >&2; exit 1
    fi
    sleep 1
done
running || { echo 'VPN process ownership check failed' >&2; exit 1; }
echo 'VPN proxy ready'
