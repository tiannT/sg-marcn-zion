#!/bin/sh
set -e
mkdir -p /tmp
chmod 1777 /tmp
echo '[PLANB_EMBEDDED_APPS] VERIFY_FILES'
sha256sum -c /etc/planb-reference-files.sha256 > /tmp/planb-reference-files-check.log
/usr/bin/busybox --help 2>&1 | head -n 1
echo '[PLANB_EMBEDDED_APPS] READY redis=7.0.15 mysql=8.0.46 sqlite=3.46.1'
exec /etc/zion-ssh.sh
