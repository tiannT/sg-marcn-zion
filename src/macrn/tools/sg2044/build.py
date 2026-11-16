#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""Build and inspect an M-mode firmware candidate; never install or flash it."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import subprocess

REPO = Path(__file__).resolve().parents[2]
UPSTREAM = "dbc1501ef48623e68ce415305c84aa994de9b0b4"


def h_extension_sites(disassembly):
    """Inspect encoded SYSTEM instructions, including numeric CSR spellings."""
    sites = []
    for line in disassembly.splitlines():
        match = re.match(r"\s*([0-9a-f]+):\s+([0-9a-f]{8})\s", line)
        if not match:
            continue
        word = int(match.group(2), 16)
        if word & 0x7f != 0x73:
            continue
        funct3, funct7, csr = (word >> 12) & 7, word >> 25, word >> 20
        h_csr = (funct3 in (1, 2, 3, 5, 6, 7) and
                 (0x200 <= csr <= 0x2ff or 0x600 <= csr <= 0x6ff or csr == 0xe12))
        h_fence = funct3 == 0 and funct7 in (0x11, 0x31)
        h_load_store = funct3 == 4
        if h_csr or h_fence or h_load_store:
            sites.append(line.strip())
    return sites


def inspect(out, prefix, embedded_guest=None):
    elf = out / "acrn.elf"
    data = elf.read_bytes()
    if data[:6] != b"\x7fELF\x02\x01":
        raise RuntimeError("Expected a little-endian ELF64 monitor")
    entry = struct.unpack_from("<Q", data, 24)[0]
    phoff = struct.unpack_from("<Q", data, 32)[0]
    phsize, phnum = struct.unpack_from("<HH", data, 54)
    segments = []
    for i in range(phnum):
        typ, flags, offset, va, pa, filesz, memsz, align = struct.unpack_from(
            "<IIQQQQQQ", data, phoff + i * phsize)
        if typ == 1:
            segments.append({"address": hex(pa), "file_bytes": filesz,
                             "memory_bytes": memsz, "end": hex(pa + memsz)})
            if pa < 0x90000000 or pa + memsz > 0x91000000:
                raise RuntimeError("Monitor segment is outside its reserved region")
    if entry != 0x90000000:
        raise RuntimeError("Runtime entry must be 0x90000000")
    wrapper = (out / "mode_entry.elf").read_bytes()
    if struct.unpack_from("<Q", wrapper, 24)[0] != 0x80000000:
        raise RuntimeError("Loader entry must be 0x80000000")
    if (out / "fw_dynamic.bin").stat().st_size > 0x200000:
        raise RuntimeError("Loader overlaps the next-stage load address")
    dis = subprocess.check_output([prefix + "objdump", "-d", str(elf)], text=True)
    (out / "disassembly.txt").write_text(dis)
    forbidden = h_extension_sites(dis)
    if forbidden:
        raise RuntimeError("H-extension access found: " + repr(forbidden[:10]))
    names = ["acrn.elf", "mode_entry.elf", "monitor.bin", "fw_dynamic.bin"]
    if embedded_guest:
        guest = (out / "bringup-guest.elf").read_bytes()
        if guest[:6] != b"\x7fELF\x02\x01" or struct.unpack_from("<Q", guest, 24)[0] != 0x82000000:
            raise RuntimeError("Embedded Guest entry must be 0x82000000")
        if not 0 < (out / "bringup-guest.bin").stat().st_size <= 0x10000:
            raise RuntimeError("Embedded Guest is outside its 64 KiB region")
        names += ["bringup-guest.elf", "bringup-guest.bin"]
    return {"upstream_commit": UPSTREAM, "runtime_entry": hex(entry),
            "loader_entry": "0x80000000", "segments": segments,
            "known_h_extension_sites": len(forbidden),
            "artifacts": {name: {"bytes": (out / name).stat().st_size,
                                  "sha256": hashlib.sha256((out / name).read_bytes()).hexdigest()}
                          for name in names}}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--platform", choices=["sg2044", "qemu"], default="sg2044")
    default_prefix = "" if os.uname().machine == "riscv64" else "riscv64-linux-gnu-"
    p.add_argument("--cross-compile", default=default_prefix)
    p.add_argument("--pcpus", type=int)
    p.add_argument("--embedded-guest", choices=["smoke", "pmp-probe"],
                   help="Single-hart diagnostic Guest; skips UEFI and never boots the disk OS")
    p.add_argument("--static-guests", choices=["smoke", "linux"],
                   help="Two static VMs, one Hart each; Linux mode requires a matching payload bundle")
    p.add_argument("--static-probe-vm", type=int, choices=[0, 1], default=0)
    p.add_argument("--static-probe-access", choices=["read", "write", "execute"], default="read")
    p.add_argument("--live-pmp-test", action="store_true", help="Enable fixed recoverable probes and fault audit in normal dual Linux")
    p.add_argument("--plan-b-services", action="store_true", help="Enable bounded virtio I/O and a checked vCPU backend channel")
    p.add_argument("--plan-b-enclave", action="store_true", help="Enable actual no-H PMP enclaves in VM1; implies --plan-b-services")
    p.add_argument("--plan-b-runtime", action="store_true", help="Enable controlled VM1 stop/clean/reload/start from a unified bundle")
    p.add_argument("--jobs", type=int, default=4)
    p.add_argument("--output-dir", type=Path)
    p.add_argument("--inspect-only", action="store_true")
    args = p.parse_args()
    if args.plan_b_runtime:
        args.plan_b_enclave = True
    if args.plan_b_enclave:
        args.plan_b_services = True
    if args.pcpus is not None and not 1 <= args.pcpus <= 64:
        p.error("--pcpus must be in 1..64")
    if args.embedded_guest and args.pcpus != 1:
        p.error("--embedded-guest requires --pcpus 1")
    if args.static_guests and (args.pcpus != 2 or args.embedded_guest):
        p.error("--static-guests requires --pcpus 2 and no --embedded-guest")
    if args.live_pmp_test and args.static_guests != "linux":
        p.error("--live-pmp-test requires --static-guests linux")
    if args.plan_b_services and args.static_guests != "linux":
        p.error("--plan-b-services requires --static-guests linux")
    out = (args.output_dir or REPO / "hypervisor/build" / args.platform).resolve()
    command = ["make", "ARCH=riscv", "PLATFORM=" + args.platform,
               "CONFIG_MACRN=1", "CROSS_COMPILE=" + args.cross_compile,
               "HV_OBJDIR=" + str(out), "-j" + str(args.jobs)]
    if args.pcpus is not None:
        command.append("PCPU_COUNT=" + str(args.pcpus))
    if args.live_pmp_test:
        command.append("LIVE_PMP_TEST=1")
    if args.plan_b_services:
        command.append("PLAN_B_SERVICES=1")
    if args.plan_b_enclave:
        command.append("PLAN_B_ENCLAVE=1")
    if args.plan_b_runtime:
        command.append("PLAN_B_RUNTIME=1")
    if args.embedded_guest:
        command.append("BRINGUP_GUEST=" + args.embedded_guest)
    if args.static_guests:
        command.append("STATIC_GUESTS=" + args.static_guests)
        if args.static_guests == "smoke":
            command += ["STATIC_PROBE_VM=" + str(args.static_probe_vm),
                        "STATIC_PROBE_ACCESS=" + args.static_probe_access]
    if not args.inspect_only:
        out.mkdir(parents=True, exist_ok=True)
        with (out / "build.log").open("w") as log:
            result = subprocess.run(command, cwd=REPO / "hypervisor", stdout=log,
                                    stderr=subprocess.STDOUT)
        if result.returncode:
            print((out / "build.log").read_text()[-6000:])
            raise SystemExit(result.returncode)
    result = inspect(out, args.cross_compile, args.embedded_guest)
    result.update(platform=args.platform, build_command=command,
                  board_name=("SG2044_EVB" if args.platform == "sg2044" else "QEMU virt"),
                  embedded_guest=args.embedded_guest,
                  static_guests=args.static_guests,
                  live_pmp_test=args.live_pmp_test,
                  plan_b_services=args.plan_b_services,
                  plan_b_enclave=args.plan_b_enclave,
                  plan_b_runtime=args.plan_b_runtime,
                  next_stage=("VM0 plus controlled Asterinas/Linux VM1 lifecycle from one unified bundle"
                              if args.plan_b_runtime else "two statically partitioned Guests"
                              if args.static_guests else "embedded diagnostic Guest at 0x82000000"
                              if args.embedded_guest else "vendor-provided S-mode next stage"),
                  compiler=subprocess.check_output(
                      [args.cross_compile + "gcc", "--version"], text=True).splitlines()[0],
                  options=(out / "build-options.txt").read_text())
    (out / "build-manifest.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
