#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""Package two Linux payloads for ZSBL's SG2044.fd load slot; no device writes."""
import argparse
import hashlib
import json
from pathlib import Path
import struct

HEADER_SIZE = 40 + 2 * 48
MAX_SIZE = 0x88000000 - 0x80200000


def build_bundle(kernels, initrds, cmdlines, output, board_dtb=None):
    data = bytearray(HEADER_SIZE)
    chunks = {}
    entries = []
    manifest = []

    def add(blob):
        digest = hashlib.sha256(blob).hexdigest()
        if digest not in chunks:
            data.extend(b"\0" * (-len(data) % 4096))
            chunks[digest] = len(data)
            data.extend(blob)
        return chunks[digest], len(blob), digest

    for vm in range(2):
        kernel = Path(kernels[vm]).read_bytes()
        if len(kernel) < 64 or kernel[56:60] != b"RSC\x05":
            raise ValueError("Expected an uncompressed RISC-V Linux Image")
        text_offset, image_size = struct.unpack_from("<QQ", kernel, 8)
        if text_offset != 0x200000 or image_size > 0xfe00000 or len(kernel) > 0xfe00000:
            raise ValueError("Linux Image does not fit the fixed 2 MiB entry layout")
        ko, ks, kh = add(kernel)
        io, size, ih = add(Path(initrds[vm]).read_bytes()) if initrds[vm] else (0, 0, None)
        cmd = cmdlines[vm].encode() + b"\0"
        if len(cmd) > 4096 or b"\0" in cmd[:-1]:
            raise ValueError("Invalid Linux command line")
        co, cs, _ = add(cmd)
        entries.append((ko, ks, io, size, co, cs))
        manifest.append(dict(vm=vm, kernel_sha256=kh, kernel_bytes=ks,
                             image_size=image_size, initrd_sha256=ih, initrd_bytes=size,
                             cmdline=cmdlines[vm]))
    fdt_offset, fdt_size, fdt_hash = (0, 0, None)
    if board_dtb:
        fdt = Path(board_dtb).read_bytes()
        if len(fdt) < 40 or struct.unpack_from(">II", fdt) != (0xd00dfeed, len(fdt)):
            raise ValueError("Invalid board DTB")
        fdt_offset, fdt_size, fdt_hash = add(fdt)
    if len(data) > MAX_SIZE:
        raise ValueError(f"Bundle exceeds the load slot: {len(data)} > {MAX_SIZE}")
    struct.pack_into("<8sIIQ", data, 0, b"MACRN2VM", 1, 2, len(data))
    struct.pack_into("<QQ", data, 24, fdt_offset, fdt_size)
    for vm, entry in enumerate(entries):
        struct.pack_into("<6Q", data, 40 + vm * 48, *entry)
    output = Path(output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(data)
    result = dict(format="MACRN2VM v1", load_address="0x80200000",
                  load_end=hex(0x80200000 + len(data)), maximum_bytes=MAX_SIZE,
                  bytes=len(data), sha256=hashlib.sha256(data).hexdigest(), vms=manifest,
                  board_dtb_sha256=fdt_hash, board_dtb_bytes=fdt_size,
                  security="Hashes identify artifacts; they do not authenticate the boot chain")
    output.with_suffix(output.suffix + ".json").write_text(json.dumps(result, indent=2) + "\n")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for vm in range(2):
        parser.add_argument(f"--vm{vm}-kernel", type=Path, required=True)
        parser.add_argument(f"--vm{vm}-initrd", type=Path)
        parser.add_argument(f"--vm{vm}-cmdline", required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--board-dtb", type=Path, help="Complete Linux board DTB template, including PCIe")
    args = parser.parse_args()
    print(json.dumps(build_bundle([args.vm0_kernel, args.vm1_kernel],
          [args.vm0_initrd, args.vm1_initrd], [args.vm0_cmdline, args.vm1_cmdline], args.output, args.board_dtb), indent=2))


if __name__ == "__main__":
    main()
