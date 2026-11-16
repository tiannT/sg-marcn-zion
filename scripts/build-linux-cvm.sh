#!/usr/bin/env bash
set -euo pipefail
project_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
kernel="$project_root/src/linux-cvm-kernel"
output=${PLANB_LINUX_OUTPUT:-$project_root/build/linux-cvm}
compiler_prefix=${CROSS_COMPILE-riscv64-linux-gnu-}
[[ -f "$kernel/Makefile" ]] || { printf 'Initialize Linux first: bash scripts/init-submodules.sh\n' >&2; exit 1; }
mkdir -p -- "$output"
cp -- "$project_root/configs/linux-cvm.config" "$output/.config"
export ARCH=riscv CROSS_COMPILE="$compiler_prefix"
export KBUILD_BUILD_USER=planb KBUILD_BUILD_HOST=sg2044
export KBUILD_BUILD_TIMESTAMP='Thu Nov 12 00:00:00 UTC 2026'
make -C "$kernel" O="$output" olddefconfig
make -C "$kernel" O="$output" -j"${JOBS:-2}" Image modules
printf 'Kernel: %s/arch/riscv/boot/Image\n' "$output"
