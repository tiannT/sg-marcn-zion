#!/bin/sh
# SPDX-License-Identifier: BSD-3-Clause
# Noninteractive init for the bounded Plan B acceptance environment.
echo '[PLANB_ASTERINAS_INIT] BEGIN'
mount -t sysfs none /sys
echo "[PLANB_ASTERINAS_INIT] SYSFS_EXIT=$?"
mount -t proc none /proc
echo "[PLANB_ASTERINAS_INIT] PROC_EXIT=$?"
mount -t cgroup2 none /sys/fs/cgroup
echo "[PLANB_ASTERINAS_INIT] CGROUP_EXIT=$?"
mount -t configfs none /sys/kernel/config
echo "[PLANB_ASTERINAS_INIT] CONFIGFS_EXIT=$?"
echo '[PLANB_ASTERINAS_INIT] START_TEST_SERVICES'
exec /bin/sh /test/planb-asterinas-ssh.sh
