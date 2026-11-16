#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""Inject upstream handoff faults at the QEMU M-mode entry using GDB."""
import argparse
import json
from pathlib import Path
import socket
import subprocess
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--before-firmware", required=True, type=Path)
    parser.add_argument("--embedded-firmware", required=True, type=Path)
    parser.add_argument("--normal-firmware", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    args = parser.parse_args()
    out = args.output_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    unused = out / "unused-next-stage.bin"
    unused.write_bytes(bytes.fromhex("6f000000"))
    cases = [
        ("before-non-s-mode", args.before_firmware, "mode", "old-rejection"),
        ("embedded-null-info", args.embedded_firmware, "null", "pass"),
        ("embedded-invalid-magic", args.embedded_firmware, "magic", "pass"),
        ("embedded-invalid-version", args.embedded_firmware, "version", "pass"),
        ("embedded-non-s-mode", args.embedded_firmware, "mode", "pass"),
        ("embedded-invalid-fdt", args.embedded_firmware, "fdt", "fdt-rejection"),
        ("normal-non-s-mode", args.normal_firmware, "mode", "mode-rejection"),
        ("normal-null-info", args.normal_firmware, "null", "null-rejection"),
        ("normal-bss-overlap-info", args.normal_firmware, "bss-overlap", "snapshot-pass"),
    ]
    results = {}
    for name, firmware, fault, expected in cases:
        with socket.socket() as probe:
            probe.bind(("127.0.0.1", 0))
            port = probe.getsockname()[1]
        command = ["qemu-system-riscv64", "-M", "virt", "-cpu",
                   "rv64,h=false,zbb=true,sv57=false", "-smp", "1", "-m", "1G",
                   "-nographic", "-monitor", "none", "-S",
                   "-gdb", "tcp:127.0.0.1:" + str(port),
                   "-bios", str(firmware.resolve()), "-kernel", str(unused)]
        logfile = out / (name + ".log")
        with logfile.open("w") as stream:
            process = subprocess.Popen(command, stdout=stream, stderr=subprocess.STDOUT)
            try:
                deadline = time.monotonic() + 5
                while True:
                    listeners = Path("/proc/net/tcp").read_text().splitlines()[1:]
                    ready = any(line.split()[1].endswith(":" + f"{port:04X}")
                                and line.split()[3] == "0A" for line in listeners)
                    if ready:
                        break
                    if process.poll() is not None or time.monotonic() > deadline:
                        raise RuntimeError("QEMU debugger listener did not start")
                    time.sleep(0.03)
                steps = ["set architecture riscv:rv64",
                         "target remote 127.0.0.1:" + str(port),
                         "hbreak *0x80000000", "continue"]
                if fault == "null":
                    steps += ["set $a2 = 0"]
                elif fault == "fdt":
                    steps += ["set $a1 = 0"]
                else:
                    fields = [0x4942534f, 2, 0x80200000, 1, 0, 0]
                    if fault == "magic":
                        fields[0] = 0
                    elif fault == "version":
                        fields[1] = 99
                    elif fault == "mode":
                        fields[3] = 3
                    pointer = 0x90097210 if fault == "bss-overlap" else 0x83001000
                    for index, value in enumerate(fields):
                        steps += [f"set {{unsigned long long}}0x{pointer + index * 8:x} = 0x{value:x}"]
                    steps += [f"set $a2 = 0x{pointer:x}"]
                steps += ["delete breakpoints", "detach"]
                debugger = ["gdb-multiarch", "-q", "-nx", "-batch"]
                for step in steps:
                    debugger += ["-ex", step]
                result = subprocess.run(debugger, text=True, stdout=subprocess.PIPE,
                                        stderr=subprocess.STDOUT, timeout=12)
                (out / (name + ".gdb.log")).write_text(result.stdout)
                if result.returncode:
                    raise RuntimeError("GDB handoff injection failed: " + result.stdout)
                try:
                    code = process.wait(timeout=4 if expected == "pass" else 1)
                except subprocess.TimeoutExpired:
                    code = "timeout"
                stream.flush()
                log = logfile.read_text(errors="replace")
                rejections = {
                    "old-rejection": "valid fw_dynamic_info with an S-mode payload is required",
                    "fdt-rejection": "missing, invalid or oversized device tree",
                    "mode-rejection": "fw_dynamic_info next stage is not S-mode",
                    "null-rejection": "missing fw_dynamic_info",
                }
                if expected == "pass":
                    passed = (code == 0 and
                              "[BRINGUP] EMBEDDED_HANDOFF_SELF_CONTAINED" in log and
                              "[BRINGUP GUEST] SMOKE_PASS_PARK" in log and
                              "[BRINGUP GUEST] FAIL" not in log and
                              log.count("[BRINGUP GUEST] TIMER_TICK") == 3)
                elif expected == "snapshot-pass":
                    passed = (code == "timeout" and
                              "[BOOT] DYNAMIC_MAGIC=0x000000004942534f" in log and
                              "[BOOT] DYNAMIC_ENTRY=0x0000000080200000" in log and
                              "successfully launched" in log and "boot rejected" not in log)
                else:
                    passed = (code == "timeout" and rejections[expected] in log and
                              "[BRINGUP GUEST] S_MODE_ENTERED" not in log)
                results[name] = dict(expected=expected, fault=fault, exit=code,
                                     passed=passed, qemu_command=command)
            finally:
                if process.poll() is None:
                    process.terminate()
                    process.wait(timeout=5)
    (out / "results.json").write_text(json.dumps(results, indent=2) + "\n")
    print(json.dumps(results, indent=2))
    raise SystemExit(0 if all(case["passed"] for case in results.values()) else 1)


if __name__ == "__main__":
    main()
