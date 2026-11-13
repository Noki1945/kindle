#!/bin/sh
# Read-only, manual execution on an already accessible device. No serial numbers or network credentials.
set -u
printf '%s\n' '=== Kindle Lite read-only probe ==='
uname -a
printf '%s\n' '=== CPU / RAM ==='
sed -n '/^Processor/p; /^model name/p; /^Hardware/p; /^CPU part/p' /proc/cpuinfo
sed -n '/^MemTotal:/p' /proc/meminfo
printf '%s\n' '=== ABI loaders ==='
for path in /lib/ld-linux-armhf.so.3 /lib/ld-linux.so.3; do
    if [ -e "$path" ]; then ls -l "$path"; fi
done
printf '%s\n' '=== Framebuffer / input capabilities ==='
for path in /sys/class/graphics/fb0/name /sys/class/graphics/fb0/virtual_size /proc/bus/input/devices; do
    if [ -r "$path" ]; then cat "$path"; fi
done
printf '%s\n' '=== Power capabilities (no changes) ==='
if [ -r /sys/power/state ]; then cat /sys/power/state; fi
for command in lipc-get-prop lipc-wait-event fbink; do command -v "$command" || true; done
printf '%s\n' '=== Battery capacity if exposed by kernel ==='
for path in /sys/class/power_supply/*/capacity; do
    if [ -r "$path" ]; then printf '%s: ' "$path"; cat "$path"; fi
done
