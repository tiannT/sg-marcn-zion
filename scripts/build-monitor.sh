#!/usr/bin/env bash
set -euo pipefail
project_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
compiler_prefix=${CROSS_COMPILE-riscv64-linux-gnu-}
exec "${PYTHON:-python3}" "$project_root/src/macrn/tools/sg2044/build.py" \
  --platform sg2044 --pcpus 2 --static-guests linux \
  --live-pmp-test --plan-b-services --plan-b-enclave --plan-b-runtime \
  --cross-compile "$compiler_prefix" \
  --output-dir "${PLANB_MONITOR_OUTPUT:-$project_root/build/monitor}" "$@"
