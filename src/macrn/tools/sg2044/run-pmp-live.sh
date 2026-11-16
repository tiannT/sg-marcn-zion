#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Run on VM0 after booting the matched B2 firmware and dual-Linux bundle.
set -u -o pipefail
umask 077
task_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd) || exit 1
task_tool="$task_dir/macrn-pmp-test"
task_disk=${1:-/dev/nvme0n1p2}
task_log=$(mktemp -d /tmp/macrn-pmp-live.XXXXXXXX) || exit 1
exec > >(tee "$task_log/session.log") 2>&1
printf 'PMP test logs: %s\n' "$task_log"
if [[ $EUID != 0 || ! -x "$task_tool" || ! -b "$task_disk" ]]; then
    printf 'Run as root; keep macrn-pmp-test beside this script; select an existing NVMe block device.\n'
    exit 2
fi
# Refuse old firmware before even starting a /dev/mem mapping.
if [[ ! -r /proc/device-tree/chosen/macrn,pmp-live-test-abi ]]; then
    printf 'NEED_B2_FIRMWARE: replace the matched fw_dynamic.bin and SG2044.fd first.\n'
    exit 2
fi
cat /proc/sys/kernel/random/boot_id > "$task_log/boot-id.before"
cat /proc/interrupts > "$task_log/interrupts.before"
cat /proc/cmdline > "$task_log/cmdline"
uname -a
findmnt / || exit 1
ip -br address
dmesg > "$task_log/dmesg.before" || exit 1
printf 'NVMe read before PMP probes (16 MiB, O_DIRECT, read only):\n'
timeout 45 "$task_tool" --health-nvme "$task_disk" || exit 1
"$task_tool" --run || exit 1
printf 'NVMe read after PMP probes (16 MiB, O_DIRECT, read only):\n'
timeout 45 "$task_tool" --health-nvme "$task_disk" || exit 1
cat /proc/sys/kernel/random/boot_id > "$task_log/boot-id.after"
cat /proc/interrupts > "$task_log/interrupts.after"
dmesg > "$task_log/dmesg.after" || exit 1
cmp -s "$task_log/boot-id.before" "$task_log/boot-id.after" || {
    printf 'FAIL: Linux boot ID changed.\n'; exit 1;
}
task_bad_pattern='nvme.*(timeout|reset|abort)|Kernel panic|Oops -'
task_bad_before=$(grep -Ec "$task_bad_pattern" "$task_log/dmesg.before" || true)
task_bad_after=$(grep -Ec "$task_bad_pattern" "$task_log/dmesg.after" || true)
if [[ "$task_bad_before" != "$task_bad_after" ]]; then
    printf 'FAIL: new NVMe/kernel errors, or kernel log wrapped; inspect the saved dmesg files.\n'
    exit 1
fi
printf 'NORMAL_DUAL_LINUX_PMP_AND_NVME_CHECKS_PASSED\n'
printf 'Keep this SSH session open and reconnect from a second terminal to confirm network service.\n'
printf 'PMP test logs: %s\n' "$task_log"
