#!/usr/bin/env bash
set -euo pipefail
project_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
compiler_prefix=${CROSS_COMPILE-riscv64-linux-gnu-}
compiler=${CC:-${compiler_prefix}gcc}
output=${PLANB_TOOLS_OUTPUT:-$project_root/build/tools}
command -v "$compiler" >/dev/null || { printf 'Compiler not found: %s\n' "$compiler" >&2; exit 1; }
mkdir -p -- "$output"
include="$output/include"
mkdir -p -- "$include/asm"
# Export only shared UAPI headers. The monitor's asm/types.h must not shadow
# the target libc/Linux asm/types.h while compiling userspace tools.
for header in planb_runtime.h planb_abi.h planb_console.h pmp_live.h; do
  cp -- "$project_root/src/macrn/hypervisor/include/arch/riscv/asm/$header" "$include/asm/"
done
sources=("$project_root/src/macrn/tools/sg2044/planb_guestctl.c"
         "$project_root/src/macrn/tools/sg2044/planb_backend.c"
         "$project_root/src/macrn/tests/riscv/linux-pmp-live.c"
         "$project_root/runtime/host-driver/tvm-control.c")
names=(planb-guestctl planb-backend macrn-pmp-test tvm-control)
for index in "${!sources[@]}"; do
  "$compiler" -std=gnu11 -O2 -Wall -Wextra -static -I "$include" \
    "${sources[$index]}" -o "$output/${names[$index]}"
done
cp -- "$project_root/runtime/planb-runtime.sh" "$output/planb-runtime.sh"
chmod 755 "$output/planb-runtime.sh"
printf 'Tools built in %s\n' "$output"
