#!/bin/sh
set -e
export PATH=/usr/sbin:/usr/bin:/sbin:/bin
mount -t devtmpfs devtmpfs /dev
mkdir -p /dev/pts /dev/shm /proc /sys /run /tmp
mount -t proc proc /proc
mount -t sysfs sysfs /sys
mount -t tmpfs tmpfs /dev/shm
/usr/bin/macrn-pmp-test --service </dev/null > /tmp/pmp-service.log 2>&1 &
printf '%s\n' "$!" > /run/pmp-service.pid
exec /etc/zion-ssh.sh
