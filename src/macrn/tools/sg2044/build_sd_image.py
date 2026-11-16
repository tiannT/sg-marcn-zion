#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""Create a file-load SD candidate; never opens or writes a physical device."""
import argparse
import hashlib
import json
import lzma
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import zipfile


def sha(path):
    h = hashlib.sha256()
    with path.open("rb") as f:
        for b in iter(lambda: f.read(4 * 1024 * 1024), b""):
            h.update(b)
    return h.hexdigest()


def run(command):
    return subprocess.run(command, check=True, text=True,
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT).stdout


def chs(lba):
    cylinder, remainder = divmod(lba, 255 * 63)
    head, sector = divmod(remainder, 63)
    if cylinder > 1023:
        return b"\xfe\xff\xff"
    sector += 1
    return bytes((head, sector | ((cylinder >> 2) & 0xc0), cylinder & 0xff))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--components", required=True, type=Path)
    p.add_argument("--output", required=True, type=Path)
    p.add_argument("--mtools-dir", type=Path, default=Path("/usr/bin"))
    p.add_argument("--label", default="m-ACRN SD candidate")
    args = p.parse_args()
    src = args.components.resolve()
    image = args.output.resolve()
    if image.suffix != ".img" or str(image).startswith("/dev/"):
        p.error("Output must be a regular .img file, not a device")
    if image.exists():
        p.error("Output exists; use a new output path")
    for name in ("fsbl.bin", "zsbl.bin", "fw_dynamic.bin", "SG2044.fd"):
        if not (src / name).is_file():
            p.error(f"Missing component: {name}")
    files = sorted([src / n for n in ("fsbl.bin", "zsbl.bin", "fw_dynamic.bin", "SG2044.fd")]
                   + list(src.glob("sg2044-*.dtbo")))
    if not list(src.glob("sg2044-*.dtbo")):
        p.error("No SG2044 device-tree overlays supplied")
    image.parent.mkdir(parents=True, exist_ok=True)
    image_bytes = 256 * 1024 * 1024
    partition_start = 2048
    sector_count = image_bytes // 512 - partition_start
    mcopy = str(args.mtools_dir / "mcopy")
    mmd = str(args.mtools_dir / "mmd")
    tool_log = []
    with tempfile.TemporaryDirectory(prefix="sg2044-sd-", dir=image.parent) as temp:
        temp = Path(temp)
        fat = temp / "partition.fat"
        with fat.open("wb") as f:
            f.truncate(sector_count * 512)
        tool_log.append(run(["mkfs.vfat", "-F", "32", "-s", "1", "-h",
                             str(partition_start), "-n", "EFI", str(fat)]))
        tool_log.append(run([mmd, "-i", str(fat), "::/riscv64"]))
        for f in files:
            tool_log.append(run([mcopy, "-m", "-i", str(fat), str(f), "::/riscv64/" + f.name]))
        tool_log.append(run(["fsck.fat", "-n", str(fat)]))
        mbr = bytearray(512)
        mbr[440:444] = bytes.fromhex("4443414d")
        entry = struct.pack("<B3sB3sII", 0, chs(partition_start), 0x0c,
                            chs(partition_start + sector_count - 1),
                            partition_start, sector_count)
        mbr[446:462] = entry
        mbr[510:512] = b"\x55\xaa"
        with image.open("xb") as f:
            f.write(mbr)
            f.seek(partition_start * 512)
            with fat.open("rb") as part:
                shutil.copyfileobj(part, f, 4 * 1024 * 1024)
        assert image.stat().st_size == image_bytes
        extract = temp / "readback"
        extract.mkdir()
        readback = {}
        for f in files:
            recovered = extract / f.name
            run([mcopy, "-i", str(image) + "@@" + str(partition_start * 512),
                 "::/riscv64/" + f.name, str(recovered)])
            actual = sha(recovered)
            expected = sha(f)
            if actual != expected:
                raise RuntimeError(f"SD image readback mismatch: {f.name}")
            readback[f.name] = {"bytes": f.stat().st_size, "sha256": expected}
    compressed = image.with_suffix(".img.xz")
    with image.open("rb") as f, lzma.open(compressed, "wb", preset=6) as out:
        shutil.copyfileobj(f, out, 4 * 1024 * 1024)
    with lzma.open(compressed, "rb") as f:
        h = hashlib.sha256()
        for b in iter(lambda: f.read(4 * 1024 * 1024), b""):
            h.update(b)
    if h.hexdigest() != sha(image):
        raise RuntimeError("XZ round-trip validation failed")
    card_zip = image.with_suffix(".card-files.zip")
    with zipfile.ZipFile(card_zip, "w", zipfile.ZIP_DEFLATED) as z:
        for f in files:
            z.write(f, "riscv64/" + f.name)
    report = {
        "label": args.label, "image": str(image), "bytes": image_bytes,
        "sha256": sha(image), "compressed_image": str(compressed),
        "compressed_bytes": compressed.stat().st_size,
        "compressed_sha256": sha(compressed), "card_files_zip": str(card_zip),
        "layout_reference": "sophgo/bootloader-riscv build_rv_firmware_image",
        "partition_table": "MBR", "partition_start_sector": partition_start,
        "partition_sectors": sector_count, "partition_type": "0x0c",
        "filesystem": "FAT32", "volume_label": "EFI",
        "component_readback": readback, "filesystem_check_passed": True,
        "compressed_roundtrip_passed": True, "hardware_boot_verified": False,
        "contains_linux_rootfs": (src / "SG2044.fd").read_bytes()[:8] == b"MACRN2VM",
        "contains_nvme_installation": False,
        "contains_flash_upgrade_package": False,
        "note": "File-load SD layout; this is not a signed firmware.bin Flash upgrade.",
    }
    image.with_suffix(".manifest.json").write_text(json.dumps(report, indent=2) + "\n")
    image.with_suffix(".build.log").write_text("\n".join(tool_log))
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
