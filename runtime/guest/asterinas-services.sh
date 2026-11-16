#!/bin/sh
echo ASTERINAS_USERLAND_BEGIN
uname -a
id
cat /proc/version
# The pinned kernel initializes its virtio interface as 10.0.2.15/24.
echo '[PLANB_ASTERINAS] REAL_VIRTIO_NETWORK_CONFIGURED'
/usr/sbin/dropbear -F -E -s -r /etc/dropbear/host_key -p 10.0.2.15:22 &
echo ASTERINAS_SQLITE_BEGIN
/benchmark/bin/sqlite-speedtest1 --size 10
echo "ASTERINAS_SQLITE_EXIT=$?"
while :; do sleep 10; done
