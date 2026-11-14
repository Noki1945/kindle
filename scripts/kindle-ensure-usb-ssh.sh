#!/bin/sh
# Optional recovery for USBNetLite's boot-time daemon startup. Does not enable
# SSH on Wi-Fi, alter keys, or permit passwords.
[ -f /mnt/us/usbnetlite/auto ] || exit 0
[ -x /mnt/us/usbnetlite/bin/dropbearmulti ] || exit 0
# USB can appear after the display job. Run this helper in the background so
# waiting for the gadget never delays the user interface.
attempt=0
while [ ! -e /sys/class/net/usb0 ]; do
    attempt=$((attempt + 1))
    [ "$attempt" -lt 60 ] || exit 1
    sleep 1
done
ifconfig usb0 192.168.15.244 up || exit 1
if awk '$2 ~ /:0016$/ && $4 == "0A" { found=1 } END { exit !found }' /proc/net/tcp; then exit 0; fi
/mnt/us/usbnetlite/bin/dropbearmulti dropbear -R -H /mnt/us -P /var/run/kindle-lite-ssh.pid -K 15 -p 22 -l usb0 -s
