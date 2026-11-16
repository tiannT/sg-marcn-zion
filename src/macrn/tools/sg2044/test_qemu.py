#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""Functional no-H tests for QEMU virt; these do not emulate SG2044 devices."""
import argparse
import json
from pathlib import Path
import stat
import subprocess

REPO = Path(__file__).resolve().parents[2]


def run_qemu(args, kernel, extra, timeout, name):
    cpu = "rv64,h=false,zbb=true,sv57=false"
    if args.svpbmt:
        cpu += ",svpbmt=true"
    cmd = [args.qemu, "-M", "virt", "-cpu", cpu,
           "-smp", str(args.harts), "-m", "1G", "-bios", str(args.monitor_dir / "fw_dynamic.bin"),
           "-kernel", str(kernel), "-nographic", "-monitor", "none", *extra]
    try:
        p = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                           text=True, timeout=timeout)
        code, log = p.returncode, p.stdout
    except subprocess.TimeoutExpired as ex:
        code = "timeout"
        log = ex.stdout or b""
        if isinstance(log, bytes):
            log = log.decode(errors="replace")
    (args.output_dir / (name + ".log")).write_text(log)
    return {"command": cmd, "exit": code}, log


def initramfs(init, destination):
    archive = bytearray()
    def add(name, mode, data=b"", major=0, minor=0, ino=1):
        name = name.encode() + b"\0"
        fields = [ino, mode, 0, 0, 1, 0, len(data), 0, 0, major, minor, len(name), 0]
        archive.extend(b"070701" + b"".join(f"{n:08x}".encode() for n in fields) + name)
        archive.extend(b"\0" * (-len(archive) % 4))
        archive.extend(data)
        archive.extend(b"\0" * (-len(archive) % 4))
    add("dev", stat.S_IFDIR | 0o755)
    add("dev/console", stat.S_IFCHR | 0o600, major=5, minor=1, ino=2)
    add("init", stat.S_IFREG | 0o755, init.read_bytes(), ino=3)
    add("TRAILER!!!", 0, ino=4)
    destination.write_bytes(archive)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--monitor-dir", type=Path, default=REPO / "hypervisor/build/qemu")
    p.add_argument("--output-dir", type=Path, default=REPO / "hypervisor/build/qemu-tests")
    p.add_argument("--cross-compile", default="riscv64-linux-gnu-")
    p.add_argument("--qemu", default="qemu-system-riscv64")
    p.add_argument("--harts", type=int, default=5)
    p.add_argument("--svpbmt", action="store_true", help="Expose Svpbmt memory attributes")
    p.add_argument("--linux-image", type=Path, help="Uncompressed RISC-V Linux Image")
    p.add_argument("--linux-timeout", type=int, default=30)
    args = p.parse_args()
    args.monitor_dir = args.monitor_dir.resolve()
    args.output_dir = args.output_dir.resolve()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    cc, objcopy = args.cross_compile + "gcc", args.cross_compile + "objcopy"
    results = {}
    cases = [("smoke", []), ("private-deny", ["-DPROBE_MONITOR_MEMORY"])]
    if args.svpbmt:
        cases.append(("svpbmt", ["-DPROBE_SVPBMT"]))
    for name, define in cases:
        elf, binary = [args.output_dir / (name + ext) for ext in [".elf", ".bin"]]
        subprocess.run([cc, "-nostdlib", "-static", "-march=rv64gc", "-mabi=lp64d",
                        "-Wl,--build-id=none", "-Wl,-T," + str(REPO / "tests/riscv/guest-smoke.lds"),
                        "-x", "assembler-with-cpp", *define,
                        str(REPO / "tests/riscv/guest-smoke.s"), "-o", str(elf)], check=True)
        subprocess.run([objcopy, "-O", "binary", str(elf), str(binary)], check=True)
        result, log = run_qemu(args, binary, [], 6 if name == "private-deny" else 15, name)
        if name != "private-deny":
            result["passed"] = result["exit"] == 0 and all(x in log for x in
                ["GUEST_SMODE_ENTERED", "GUEST_SBI_BASE_OK", "GUEST_TIMER_IRQ_OK"]) and "GUEST_FAILED" not in log
            if name == "svpbmt":
                result["passed"] = result["passed"] and "GUEST_SVPBMT_OK" in log
        else:
            result["passed"] = (result["exit"] == "timeout" and "GUEST_SBI_BASE_OK" in log
                and "Guest attempted monitor-memory access: 90000000" in log
                and "GUEST_PRIVATE_ACCESS_BYPASSED" not in log)
        results[name] = result
    if args.linux_image:
        init = args.output_dir / "init"
        subprocess.run([cc, "-nostdlib", "-static", "-fno-pie", "-ffreestanding", "-fno-builtin",
                        "-mcmodel=medany", "-march=rv64gc", "-mabi=lp64d", "-Os",
                        "-Wl,--build-id=none", str(REPO / "tests/riscv/linux-init.c"),
                        str(REPO / "tests/riscv/linux-init-start.s"), "-o", str(init)], check=True)
        ramfs = args.output_dir / "initramfs.cpio"
        initramfs(init, ramfs)
        result, log = run_qemu(args, args.linux_image.resolve(), ["-initrd", str(ramfs), "-append",
            "console=ttyS0,115200 earlycon=uart8250,mmio,0x10000000 rdinit=/init maxcpus=1 loglevel=6"],
            args.linux_timeout, "linux")
        result.update(init="GUEST_LINUX_INIT_OK" in log,
                      single_vcpu="GUEST_LINUX_SINGLE_VCPU_OK" in log,
                      timer_ticks=log.count("GUEST_LINUX_TIMER_TICK"))
        result["passed"] = (result["exit"] == 0 and result["init"] and result["single_vcpu"]
            and result["timer_ticks"] == 3 and "GUEST_LINUX_TESTS_PASSED" in log
            and "GUEST_LINUX_TESTS_FAILED" not in log)
        results["linux"] = result
    (args.output_dir / "results.json").write_text(json.dumps(results, indent=2) + "\n")
    print(json.dumps(results, indent=2))
    raise SystemExit(0 if all(r["passed"] for r in results.values()) else 1)


if __name__ == "__main__":
    main()
