#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""Build the EVB Linux DTB using verified working 1.4.3 PCI resource tables."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess


def parse_tables(path):
    fields = {}
    for name, index, values in re.findall(
        r"^\s*gSophgoSG2044PlatformPkgTokenSpaceGuid\.PcdPcieHostBridgeTable\."
        r"(\w+)\[(\d+)\]\|\{([^}]+)\}", path.read_text(), re.M):
        if name in {"PcieReg", "PcieDomain", "PcieIoRanges", "PcieMem32Ranges",
                    "PcieMem64Ranges", "PciePmem64Ranges"}:
            fields[name, int(index)] = bytes(int(x.strip(), 0) for x in values.split(","))
    return fields


def words(values):
    return " ".join(f"0x{x >> 32:x} 0x{x & 0xffffffff:08x}" for x in values)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--linux-dts-dir", type=Path, required=True)
    p.add_argument("--uefi-dsc", type=Path, required=True)
    p.add_argument("--baseline-json", type=Path, required=True)
    p.add_argument("--dtc", default="dtc")
    p.add_argument("--cpp", default="cpp")
    p.add_argument("--output-dir", type=Path, required=True)
    args = p.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    tables = parse_tables(args.uefi_dsc)
    baseline = json.loads(args.baseline_json.read_text())
    iomem = baseline["iomem"]
    ecam_bases = {int(x, 16) for x in re.findall(r"^([0-9a-f]+)-[0-9a-f]+ : PCI ECAM", iomem, re.M)}
    text = ['// SPDX-License-Identifier: MIT',
            '// SOPHGO EVB template with the working 179 / 1.4.3 PCI resource map.',
            '#include "sg2044-evb.dts"']
    report = []
    kinds = [("PcieIoRanges", 0x01000000), ("PcieMem32Ranges", 0x02000000),
             ("PcieMem64Ranges", 0x03000000), ("PciePmem64Ranges", 0x43000000)]
    for index in range(5):
        regs = struct.unpack("<8Q", tables["PcieReg", index])
        domain, = struct.unpack("<I", tables["PcieDomain", index])
        if regs[6] not in ecam_bases:
            raise ValueError(f"PCI domain {domain} configuration base differs from the working board")
        node = f"/soc/pcie@{regs[0]:x}"
        ranges = []
        item = dict(domain=domain, node=node, register_regions=[list(regs[i:i+2]) for i in range(0, 8, 2)], windows=[])
        for kind, flag in kinds:
            cpu, pci, size = struct.unpack("<3Q", tables[kind, index])
            if not size:
                continue
            if kind != "PcieIoRanges":
                observed = f"{cpu:x}-{cpu + size - 1:x} : PCI Bus {domain:04x}:00"
                if observed not in iomem:
                    raise ValueError(f"Unverified resource window: {observed}")
            ranges.append(f"<0x{flag:08x} {words([pci, cpu, size])}>")
            item["windows"].append(dict(kind=kind, cpu=hex(cpu), pci=hex(pci), size=hex(size)))
        text += [f"&{{{node}}} {{", f"    linux,pci-domain = <{domain}>;",
                 "    reg = " + ", ".join("<" + words(regs[i:i+2]) + ">" for i in range(0, 8, 2)) + ";",
                 "    ranges = " + ",\n             ".join(ranges) + ";", "};"]
        report.append(item)
    source = args.output_dir / "sg2044-macrn-evb-r2.dts"
    source.write_text("\n".join(text) + "\n")
    preprocessed = source.with_suffix(".pp.dts")
    subprocess.run([args.cpp, "-nostdinc", "-undef", "-D__DTS__", "-x", "assembler-with-cpp",
                    "-I", str(args.linux_dts_dir), "-I", str(args.linux_dts_dir / "include"),
                    str(source), "-o", str(preprocessed)], check=True)
    dtb = source.with_suffix(".dtb")
    result = subprocess.run([args.dtc, "-I", "dts", "-O", "dtb", "-o", str(dtb), str(preprocessed)],
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    (args.output_dir / "dtc-build.log").write_bytes(result.stdout)
    result.check_returncode()
    record = dict(source_dsc=str(args.uefi_dsc), source_dsc_sha256=hashlib.sha256(args.uefi_dsc.read_bytes()).hexdigest(),
                  working_baseline=str(args.baseline_json), verified_against_working_board=True,
                  dtb_bytes=dtb.stat().st_size, dtb_sha256=hashlib.sha256(dtb.read_bytes()).hexdigest(), controllers=report)
    source.with_suffix(".json").write_text(json.dumps(record, indent=2) + "\n")
    print(json.dumps(record, indent=2))


if __name__ == "__main__":
    main()
