#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""Exercise the static two-Hart profile on QEMU, without the board's live disk."""
import argparse
import json
from pathlib import Path
import subprocess
import time
from build_guest_bundle import build_bundle
from test_qemu import initramfs

REPO = Path(__file__).resolve().parents[2]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--firmware", required=True, type=Path)
    p.add_argument("--output-dir", required=True, type=Path)
    p.add_argument("--linux-image", type=Path)
    p.add_argument("--cross-compile", default="riscv64-linux-gnu-")
    p.add_argument("--probe-vm", type=int, choices=[0, 1], default=0)
    p.add_argument("--probe-access", choices=["read", "write", "execute"], default="read")
    args = p.parse_args()
    out = args.output_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    kernel = out / "unused.bin"
    kernel.write_bytes(bytes.fromhex("6f000000"))
    extra = []
    if args.linux_image:
        initrds = []
        for vm in range(2):
            init = out / f"init-vm{vm}"
            subprocess.run([args.cross_compile + "gcc", "-nostdlib", "-static", "-fno-pie",
                "-ffreestanding", "-fno-builtin", "-mcmodel=medany", "-march=rv64gc", "-mabi=lp64d",
                "-Os", "-Wl,--build-id=none", f"-DSTATIC_LINUX_VM={vm}",
                str(REPO / "tests/riscv/linux-init.c"), str(REPO / "tests/riscv/linux-init-start.s"),
                "-o", str(init)], check=True)
            ramfs = out / f"initramfs-vm{vm}.cpio"
            initramfs(init, ramfs)
            initrds.append(ramfs)
        payload = out / "SG2044.fd"
        cmdline = "console=ttyS0,115200 earlycon=uart8250,mmio,0x10000000 rdinit=/init maxcpus=1 loglevel=5"
        build_bundle([args.linux_image] * 2, initrds, [cmdline] * 2, payload)
        extra = ["-device", f"loader,file={payload},addr=0x80200000,force-raw=on"]
    command = ["qemu-system-riscv64", "-M", "virt", "-cpu",
               "rv64,h=false,zbb=true,sv57=false", "-smp", "2", "-m", "4G", "-nographic",
               "-monitor", "none", "-bios", str(args.firmware.resolve()), "-kernel", str(kernel), *extra]
    results = {}
    for name, cpu_suffix in [("sstc", ""), ("no-sstc", ",sstc=false")]:
        cmd = command.copy()
        cmd[cmd.index("-cpu") + 1] += cpu_suffix
        required = ["Brought up 2 CPUs", "VM0 HART0 PMP_PRIVATE_READBACK_OK", "VM1 HART1 PMP_PRIVATE_READBACK_OK"]
        if args.linux_image:
            required += [f"[LINUX VM{i}] GUEST_LINUX_TESTS_PASSED" for i in range(2)]
            required += [f"[LINUX VM{i}] GUEST_LINUX_SINGLE_VCPU_OK" for i in range(2)]
        else:
            required += [f"[RECOVER VM{i}] SBI_BOUNDARY_OK" for i in range(2)]
            required += [f"[RECOVER VM{i}] SHARED_OK" for i in range(2)]
            required += [f"[RECOVER VM{i}] TIMER_IRQ_OK" for i in range(2)]
            required += [f"[RECOVER VM{i}] PRIVATE_CANARY_OK" for i in range(2)]
            required += [f"[RECOVER VM{i}] SHARED_AFTER_DENIAL_OK" for i in range(2)]
            required += [f"[RECOVER VM{i}] ALIVE_AFTER_DENIAL TIMER_TICKS=3" for i in range(2)]
            denied = "DENIED_PRIVATE_EXECUTE" if args.probe_access == "execute" else "DENIED_PRIVATE_ACCESS"
            required += [f"VM{args.probe_vm} {denied}",
                         f"[RECOVER VM{args.probe_vm}] ACCESS_DENIED_RECOVERED errno=-14"]
        logfile = out / f"{name}.log"
        with logfile.open("w") as stream:
            proc = subprocess.Popen(cmd, stdout=stream, stderr=subprocess.STDOUT)
            deadline = time.monotonic() + (28 if args.linux_image else 12)
            code = "timeout"
            try:
                while time.monotonic() < deadline:
                    log = logfile.read_text(errors="replace")
                    if all(marker in log for marker in required):
                        code = "verified-and-terminated"
                        break
                    if proc.poll() is not None:
                        code = proc.returncode
                        break
                    time.sleep(0.05)
            finally:
                if proc.poll() is None:
                    proc.terminate()
                proc.wait(timeout=5)
        log = logfile.read_text(errors="replace")
        result = dict(passed=all(marker in log for marker in required) and "TESTS_FAILED" not in log and "] FAIL" not in log,
                      missing=[marker for marker in required if marker not in log], exit=code, command=cmd)
        results[name] = result
    (out / "results.json").write_text(json.dumps(results, indent=2) + "\n")
    print(json.dumps(results, indent=2))
    raise SystemExit(0 if all(r["passed"] for r in results.values()) else 1)


if __name__ == "__main__":
    main()
