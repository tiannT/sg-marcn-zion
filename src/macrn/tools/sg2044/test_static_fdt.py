#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""Check real firmware's private FDT projection using the nested SG2044 tree."""
import argparse
import json
from pathlib import Path
import socket
import struct
import subprocess
import time
from build_guest_bundle import build_bundle


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--before-dir", type=Path, required=True)
    p.add_argument("--after-dir", type=Path, required=True)
    p.add_argument("--board-dtb", type=Path, required=True)
    p.add_argument("--linux-image", type=Path, required=True)
    p.add_argument("--output-dir", type=Path, required=True)
    args = p.parse_args()
    out = args.output_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    template = out / "sg2044-nested-projection-fixture.dtb"
    template.write_bytes(args.board_dtb.read_bytes())
    # The SG tree is only projected, never executed as a QEMU Linux board.
    # This compatibility tag lets the QEMU firmware validate the test fixture.
    subprocess.run(["fdtput", "-t", "s", str(template), "/", "compatible", "riscv-virtio", "sophgo,sg2044"], check=True)
    bundle = out / "projection.bundle"
    build_bundle([args.linux_image] * 2, [None] * 2, ["projection-only"] * 2, bundle, template)
    unused = out / "unused.bin"
    unused.write_bytes(bytes.fromhex("6f000000"))
    results = {}
    for name, directory in [("before", args.before_dir), ("after", args.after_dir)]:
        with socket.socket() as s:
            s.bind(("127.0.0.1", 0))
            port = s.getsockname()[1]
        command = ["qemu-system-riscv64", "-M", "virt", "-cpu", "rv64,h=false,zbb=true,sv57=false",
                   "-smp", "2", "-m", "4G", "-nographic", "-monitor", "none", "-S",
                   "-gdb", f"tcp:127.0.0.1:{port}", "-bios", str(directory.resolve() / "fw_dynamic.bin"),
                   "-kernel", str(unused), "-device", f"loader,file={bundle},addr=0x80200000,force-raw=on"]
        with (out / f"{name}.boot.log").open("w") as stream:
            proc = subprocess.Popen(command, stdout=stream, stderr=subprocess.STDOUT)
            try:
                deadline = time.monotonic() + 5
                while True:
                    ready = any(line.split()[1].endswith(":" + f"{port:04X}") and line.split()[3] == "0A"
                                for line in Path("/proc/net/tcp").read_text().splitlines()[1:])
                    if ready:
                        break
                    if proc.poll() is not None or time.monotonic() > deadline:
                        raise RuntimeError("debugger did not start")
                    time.sleep(.03)
                steps = ["set architecture riscv:rv64", f"target remote 127.0.0.1:{port}",
                         "hbreak static_vm_set_boot_info", "continue"]
                for vm in range(2):
                    steps += [f"set $dtb{vm} = static_vms[{vm}].dtb",
                              f"dump binary memory {out}/{name}-vm{vm}.raw $dtb{vm} $dtb{vm}+0x20000"]
                debugger = ["gdb-multiarch", "-q", "-nx", "-batch", str(directory.resolve() / "acrn.elf")]
                for step in steps:
                    debugger += ["-ex", step]
                result = subprocess.run(debugger, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=35)
                (out / f"{name}.gdb.log").write_bytes(result.stdout)
                result.check_returncode()
            finally:
                if proc.poll() is None:
                    proc.terminate()
                proc.wait(timeout=5)
        vms = []
        for vm in range(2):
            data = (out / f"{name}-vm{vm}.raw").read_bytes()
            magic, size = struct.unpack_from(">II", data)
            if magic != 0xd00dfeed or size > len(data):
                raise RuntimeError("invalid projected FDT dump")
            dtb = out / f"{name}-vm{vm}.dtb"
            dtb.write_bytes(data[:size])
            dts = subprocess.check_output(["dtc", "-I", "dtb", "-O", "dts", str(dtb)], stderr=subprocess.DEVNULL, text=True)
            (out / f"{name}-vm{vm}.dts").write_text(dts)
            children = subprocess.check_output(["fdtget", "-l", str(dtb), "/soc"], text=True).splitlines()
            clocks = dts.count("sg2044, pll-") + dts.count("sg2044, clk-default-rates")
            vms.append(dict(vm=vm, soc_children=children, physical_clock_declarations=clocks,
                            pci_controllers=sum(x.startswith("pcie@") for x in children)))
        if name == "before":
            passed = vms[1]["physical_clock_declarations"] > 0
        else:
            passed = (all(v["physical_clock_declarations"] == 0 for v in vms) and
                      vms[0]["pci_controllers"] == 5 and vms[1]["pci_controllers"] == 0 and
                      len(vms[1]["soc_children"]) == 2)
        results[name] = dict(passed=passed, meaning="old leak reproduced" if name == "before" else "physical clock subtrees absent",
                             vms=vms, command=command, guest_linux_executed=False)
    (out / "results.json").write_text(json.dumps(results, indent=2) + "\n")
    print(json.dumps(results, indent=2))
    raise SystemExit(0 if all(r["passed"] for r in results.values()) else 1)


if __name__ == "__main__":
    main()
