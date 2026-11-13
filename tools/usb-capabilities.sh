#!/bin/sh
# Read-only inventory for selecting a USB SSH backend. Writes one report.
umask 077
REPORT=/mnt/us/kindle-lite-usb-capabilities.txt
{
    printf '%s\n' 'KINDLE_LITE_USB_CAPABILITIES_V1'
    date
    id
    uname -a

    printf '\n%s\n' '=== SSH tools ==='
    for tool in dropbear dropbearmulti dropbearkey sshd ssh busybox usbnetwork usbnet lipc-get-prop; do
        command -v "$tool" 2>/dev/null || true
    done
    for directory in /var/local/kmc /mnt/us/libkh /mnt/us/usbnet /mnt/us/usbnetlite /usr/sbin /usr/bin /sbin /bin; do
        if [ -d "$directory" ]; then
            find "$directory" -name '*dropbear*' -o -name sshd -o -name '*usbnet*'
        fi
    done
    if command -v busybox >/dev/null 2>&1; then
        busybox --list 2>/dev/null | grep -E '^(ip|ifconfig|modprobe|insmod|telnetd|timeout)$'
    fi

    printf '\n%s\n' '=== Kernel USB modules ==='
    grep -E '(g_ether|g_file_storage|g_mass_storage|libcomposite|usb_f_|u_ether|g_multi)' /proc/modules
    for directory in /lib/modules /usr/local/bin /usr/local/sbin; do
        if [ -d "$directory" ]; then
            find "$directory" -name '*ether*.ko' -o -name '*usbnet*' -o -name '*gadget*' -o -name '*storage*.ko'
        fi
    done

    printf '\n%s\n' '=== USB controller and gadget directories ==='
    for directory in /sys/class/udc /sys/class/android_usb /sys/kernel/config/usb_gadget /sys/devices/platform/fsl-usb2-udc; do
        if [ -d "$directory" ]; then ls -la "$directory"; fi
    done
    for file in /sys/class/android_usb/android0/functions /sys/class/android_usb/android0/state; do
        if [ -r "$file" ]; then printf '%s\n' "$file"; cat "$file"; fi
    done

    printf '\n%s\n' '=== Existing USB lifecycle scripts ==='
    for directory in /etc/upstart /etc/init.d /usr/share/udhcpc; do
        if [ -d "$directory" ]; then
            find "$directory" -name '*usb*' -o -name '*dropbear*' -o -name '*ssh*'
        fi
    done
    printf '\n%s\n' '=== Interfaces ==='
    if command -v ip >/dev/null 2>&1; then ip -4 addr show; fi

    printf '\n%s\n' '=== KMC layout ==='
    if [ -d /var/local/kmc ]; then
        ls -la /var/local/kmc
        find /var/local/kmc -maxdepth 3 -name '*patch*' -o -name '*dispatch*' -o -name '*gandalf*'
    fi
    printf '\n%s\n' '=== Native USB and SSH service definitions ==='
    for file in /etc/upstart/sshd.conf /etc/upstart/usbnet-autostart.conf /etc/upstart/usbnetd.conf; do
        if [ -r "$file" ]; then printf '\n%s\n' "$file"; head -n 160 "$file"; fi
    done
    printf '\n%s\n' '=== Native USB command ==='
    ls -l /usr/sbin/usbnetwork
    if [ "$(head -c 2 /usr/sbin/usbnetwork)" = '#!' ]; then
        head -n 240 /usr/sbin/usbnetwork
    fi
    printf '\n%s\n' '=== Additional SSH locations ==='
    for directory in /usr/local /opt /var/local/kmc; do
        if [ -d "$directory" ]; then
            find "$directory" -name '*dropbear*' -o -name '*sshd*' -o -name '*usbnet*'
        fi
    done
    printf '\n%s\n' 'REPORT_COMPLETE'
} > "$REPORT" 2>&1
sync
