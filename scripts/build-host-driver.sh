#!/usr/bin/env bash
# Build against the exact target Host kernel; never force-load a module.
set -euo pipefail
project_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
kernel_build=${KERNEL_BUILD_DIR:?Set KERNEL_BUILD_DIR to the configured target Host kernel build tree}
output=${PLANB_HOST_DRIVER_OUTPUT:-$project_root/build/host-driver}
tools=${PLANB_TOOLS_OUTPUT:-$project_root/build/tools}
compiler_prefix=${CROSS_COMPILE:-riscv64-linux-gnu-}
for input in .config Module.symvers include/generated/utsrelease.h; do
    [[ -s "$kernel_build/$input" ]] || { printf 'Missing target kernel input: %s\n' "$input" >&2; exit 1; }
done
mkdir -p -- "$output" "$tools"
cp -- "$project_root/runtime/host-driver/Makefile" \
    "$project_root/runtime/host-driver/tvm-driver.c" \
    "$project_root/runtime/host-driver/kvm.c" \
    "$project_root/runtime/host-driver/native_uapi.h" "$output/"
make -C "$kernel_build" ARCH=riscv CROSS_COMPILE="$compiler_prefix" \
    M="$(cd -- "$output" && pwd)" -j"${JOBS:-2}" modules
cp -- "$output/tvm-driver.ko" "$output/kvm.ko" "$tools/"
printf 'Host modules: %s\n' "$tools"
