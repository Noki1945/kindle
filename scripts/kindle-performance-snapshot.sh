#!/bin/sh
# Read-only diagnostics: no URLs, browser profiles, keys or process arguments.
set -eu
echo '=== uptime / load ==='
cat /proc/uptime /proc/loadavg
echo '=== memory (kB) ==='
awk '/^(MemTotal|MemFree|MemAvailable|Buffers|Cached|SwapCached|SwapTotal|SwapFree|Dirty):/' /proc/meminfo
echo '=== paging counters (cumulative since boot) ==='
awk '/^(pswpin|pswpout|pgmajfault|oom_kill) /' /proc/vmstat
echo '=== swap ==='
cat /proc/swaps
echo '=== CPU policy ==='
for name in scaling_governor scaling_cur_freq scaling_min_freq scaling_max_freq; do
    path=/sys/devices/system/cpu/cpu0/cpufreq/$name
    if [ -r "$path" ]; then printf '%s=' "$name"; cat "$path"; fi
done
echo '=== largest resident processes (RSS kB, name; shared pages may overlap) ==='
for path in /proc/[0-9]*/status; do
    awk '/^Name:/ { name=$2 } /^VmRSS:/ { print $2, name }' "$path" 2>/dev/null || true
done | sort -nr | head -15
echo '=== native paint timings (historical; not touch-to-display latency) ==='
for path in /mnt/us/kindle-lite/runtime/ui-live.log /mnt/us/kindle-lite/runtime/boot.log; do
    if [ -r "$path" ]; then
        printf '%s\n' "${path##*/}"
        grep '^PAINT ' "$path" | tail -8 || true
    fi
done
