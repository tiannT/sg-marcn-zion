#!/bin/bash
set -e
export PATH=/usr/sbin:/usr/bin:/sbin:/bin
export LD_LIBRARY_PATH=/usr/local/lib:/usr/lib64:/usr/lib
mount -t devtmpfs devtmpfs /dev
mkdir -p /dev/pts /dev/shm /proc /sys /run/sshd /tmp /var/empty
mount -t proc proc /proc
mount -t sysfs sysfs /sys
mount -t devpts devpts /dev/pts
mount -t tmpfs tmpfs /dev/shm
# The project kernel configuration builds virtio drivers into the kernel.
chown -R 0:0 /root /etc/ssh /var/empty
test -b /dev/vda
test -f /sys/class/net/eth0/device/modalias
case "$(cat /sys/class/net/eth0/device/modalias)" in
    virtio:d00000001*) ;;
    *) echo 'eth0 is not the actual virtio-net device' >&2; exit 1 ;;
esac
ip link set lo up
ip link set eth0 up
ip addr replace 10.0.3.15/24 dev eth0
/usr/bin/macrn-pmp-test --service </dev/null >/tmp/pmp-service.log 2>&1 &
printf '%s\n' "$!" >/run/pmp-service.pid
printf '\n[PLANB_CVM] REAL_LINUX_VIRTIO_NETWORK_CONFIGURED\n'
/usr/sbin/sshd -D -e -f /etc/ssh/sshd_config
while true; do sleep 1; done
