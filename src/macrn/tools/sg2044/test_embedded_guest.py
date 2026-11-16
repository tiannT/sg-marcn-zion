#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""Exercise embedded-Guest handoff and PMP denial on QEMU virt, without UEFI."""
import argparse
import json
from pathlib import Path
import subprocess

REPO = Path(__file__).resolve().parents[2]
COMMON = [
    "[BRINGUP] M_ENTRY_80000000",
    "[BRINGUP] MONITOR_COPY_OK",
    "[BRINGUP] M_RUNTIME_90000000",
    "[BRINGUP] EMBEDDED_HANDOFF_SELF_CONTAINED",
    "[BRINGUP] EMBEDDED_GUEST_READY_82000000; UEFI skipped",
    "[BRINGUP] PMP_POLICY_READBACK_OK",
    "[BRINGUP GUEST] S_MODE_ENTERED",
    "[BRINGUP GUEST] FDT_OK",
    "[BRINGUP GUEST] SBI_BASE_OK",
    "[BRINGUP GUEST] TIMER_IRQ_OK",
]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--smoke-dir", type=Path, required=True)
    parser.add_argument("--probe-dir", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--qemu", default="qemu-system-riscv64")
    args = parser.parse_args()
    out = args.output_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    # This is deliberately not a usable next stage. Successful embedded Guest
    # execution proves that the vendor-provided next-stage entry was bypassed.
    unused = out / "unused-next-stage.bin"
    unused.write_bytes(bytes.fromhex("6f000000"))  # j .
    results = {}
    for name, folder, sstc, negative in [
        ("smoke-sstc", args.smoke_dir, True, False),
        ("smoke-without-sstc", args.smoke_dir, False, False),
        ("monitor-private-deny", args.probe_dir, True, True),
    ]:
        command = [args.qemu, "-M", "virt", "-cpu",
                   "rv64,h=false,zbb=true,sv57=false,sstc=" + str(sstc).lower(),
                   "-smp", "1", "-m", "1G", "-nographic", "-monitor", "none",
                   "-bios", str(folder.resolve() / "fw_dynamic.bin"),
                   "-kernel", str(unused)]
        try:
            process = subprocess.run(command, stdout=subprocess.PIPE,
                                     stderr=subprocess.STDOUT, timeout=12, text=True)
            code, log = process.returncode, process.stdout
        except subprocess.TimeoutExpired as error:
            code, log = "timeout", error.stdout or b""
            if isinstance(log, bytes):
                log = log.decode(errors="replace")
        (out / (name + ".log")).write_text(log)
        required = list(COMMON)
        if negative:
            required += ["[BRINGUP GUEST] PMP_PROBE_BEGIN",
                         "Guest attempted monitor-memory access: 90000000"]
        else:
            required += ["[BRINGUP GUEST] SMOKE_PASS_PARK"]
        absent = [marker for marker in required if marker not in log]
        failed = any(marker in log for marker in [
            "[BRINGUP GUEST] FAIL", "PMP_BYPASS_FAIL",
            "[BRINGUP] EARLY_M_TRAP", "m-ACRN boot rejected",
        ])
        passed = (not absent and not failed
                  and log.count("[BRINGUP GUEST] TIMER_TICK") == 3
                  and code == ("timeout" if negative else 0))
        results[name] = dict(command=command, exit=code, missing=absent,
                             timer_ticks=log.count("[BRINGUP GUEST] TIMER_TICK"),
                             passed=passed)
    (out / "results.json").write_text(json.dumps(results, indent=2) + "\n")
    print(json.dumps(results, indent=2))
    raise SystemExit(0 if all(item["passed"] for item in results.values()) else 1)


if __name__ == "__main__":
    main()
