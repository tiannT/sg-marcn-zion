#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Initial RAM root for the protected Linux. Its writable files are temporary.
export PATH=/usr/sbin:/usr/bin:/sbin:/bin
mount -t devtmpfs devtmpfs /dev
mkdir -p /proc /sys /run /tmp
mount -t proc proc /proc
mount -t sysfs sysfs /sys
mount -t tmpfs tmpfs /run
exec </dev/ttyS0 >/dev/ttyS0 2>&1
printf '\n[VM1] LINUX_INIT_READY\n'
uname -a
printf '[VM1] Online CPUs: '
cat /sys/devices/system/cpu/online
printf '[VM1] Memory: '
head -n 3 /proc/meminfo
printf '[VM1] RAM root is temporary; physical NVMe/NIC belong to VM0.\n'
if [ -x /usr/bin/macrn-pmp-test ] && [ -f /proc/device-tree/chosen/macrn,pmp-live-test-abi ]; then
    setsid /usr/bin/macrn-pmp-test --service </dev/null >/dev/ttyS0 2>&1 &
    printf '%s\n' "$!" >/run/macrn-pmp-service.pid
fi
while true; do
    setsid -c /bin/bash -i </dev/ttyS0 >/dev/ttyS0 2>&1
    sleep 1
done
