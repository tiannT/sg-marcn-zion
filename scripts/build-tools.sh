#!/usr/bin/env bash
set -euo pipefail
project_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
compiler_prefix=${CROSS_COMPILE-riscv64-linux-gnu-}
compiler=${CC:-${compiler_prefix}gcc}
output=${PLANB_TOOLS_OUTPUT:-$project_root/build/tools}
command -v "$compiler" >/dev/null || { printf 'Compiler not found: %s\n' "$compiler" >&2; exit 1; }
mkdir -p -- "$output"
include="$project_root/src/macrn/hypervisor/include/arch/riscv"
sources=(tools/sg2044/planb_guestctl.c tools/sg2044/planb_backend.c tests/riscv/linux-pmp-live.c)
names=(planb-guestctl planb-backend macrn-pmp-test)
for index in "${!sources[@]}"; do
  "$compiler" -std=gnu11 -O2 -Wall -Wextra -static -I "$include" \
    "$project_root/src/macrn/${sources[$index]}" -o "$output/${names[$index]}"
done
cp -- "$project_root/runtime/planb-runtime.sh" "$output/planb-runtime.sh"
chmod 755 "$output/planb-runtime.sh"
printf 'Tools built in %s\n' "$output"
