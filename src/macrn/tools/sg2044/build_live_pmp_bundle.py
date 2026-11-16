#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""Add the live PMP service to VM1's existing Linux initramfs; no media writes."""
import argparse
import gzip
import hashlib
import json
from pathlib import Path
import stat
import subprocess

from build_guest_bundle import build_bundle

REPO = Path(__file__).resolve().parents[2]


def update_cpio(blob, replacements):
    if blob[:2] == b'\x1f\x8b':
        blob = gzip.decompress(blob)
    records, pos = [], 0
    while blob[pos:pos+6] in (b'070701', b'070702'):
        h = [int(blob[pos+6+i*8:pos+14+i*8], 16) for i in range(13)]
        start = pos + 110
        name = blob[start:start+h[11]-1].decode()
        data = (start + h[11] + 3) & ~3
        if name == 'TRAILER!!!':
            break
        records.append((name, h, blob[data:data+h[6]]))
        pos = (data + h[6] + 3) & ~3
    else:
        raise ValueError('Expected an uncompressed/gzip newc initramfs')
    out, present = bytearray(), set()

    def add(name, h, contents):
        h = h.copy()
        n = name.encode() + b'\0'
        h[6], h[11], h[12] = len(contents), len(n), 0
        out.extend(b'070701' + b''.join(f'{v:08x}'.encode() for v in h) + n)
        out.extend(b'\0' * (-len(out) % 4))
        out.extend(contents)
        out.extend(b'\0' * (-len(out) % 4))

    for name, h, contents in records:
        key = name.removeprefix('./')
        if key in replacements:
            contents = replacements[key]
            h[1], h[4] = stat.S_IFREG | 0o755, 1
            present.add(key)
        add(name, h, contents)
    inode = max(h[0] for _, h, _ in records) + 1
    for name, contents in replacements.items():
        if name not in present:
            add(name, [inode, stat.S_IFREG | 0o755, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0], contents)
            inode += 1
    add('TRAILER!!!', [inode, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0], b'')
    out.extend(b'\0' * (-len(out) % 512))
    return gzip.compress(out, compresslevel=6, mtime=0)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--kernel', type=Path, required=True)
    ap.add_argument('--vm0-initrd', type=Path, required=True)
    ap.add_argument('--vm1-initrd', type=Path, required=True)
    ap.add_argument('--vm0-cmdline', required=True)
    ap.add_argument('--vm1-cmdline', required=True)
    ap.add_argument('--board-dtb', type=Path)
    ap.add_argument('--output-dir', type=Path, required=True)
    ap.add_argument('--cross-compile', default='riscv64-linux-gnu-')
    ap.add_argument('--sysroot', type=Path)
    args = ap.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    binary = args.output_dir/'macrn-pmp-test'
    command = [args.cross_compile+'gcc', '-static', '-fno-pie', '-no-pie', '-O2',
               '-Wall', '-Wextra', '-Werror', '-march=rv64gc', '-mabi=lp64d',
               '-idirafter', str(REPO/'hypervisor/include/arch/riscv')]
    if args.sysroot:
        command.append('--sysroot='+str(args.sysroot))
    command += [str(REPO/'tests/riscv/linux-pmp-live.c'), '-o', str(binary)]
    result = subprocess.run(command, capture_output=True, text=True)
    (args.output_dir/'tool-build.log').write_text(result.stdout+result.stderr)
    result.check_returncode()
    vm1 = args.output_dir/'vm1-live-pmp.img'
    vm1.write_bytes(update_cpio(args.vm1_initrd.read_bytes(), {
        'usr/bin/macrn-pmp-test': binary.read_bytes(),
        'init-vm1': (REPO/'tools/sg2044/vm1-init.sh').read_bytes(),
    }))
    bundle = build_bundle([args.kernel]*2, [args.vm0_initrd, vm1],
                          [args.vm0_cmdline, args.vm1_cmdline],
                          args.output_dir/'SG2044.fd', args.board_dtb)
    record = dict(bundle=bundle, tool_command=command,
                  tool_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
                  vm0_initrd_unchanged=True, native_tests_performed=False,
                  scope='Normal dual Linux payload; VM1 adds a userspace service only. '
                        'Both Linux kernels and original VM0 NVMe root are preserved.')
    (args.output_dir/'live-bundle.json').write_text(json.dumps(record, indent=2)+'\n')
    print(json.dumps(record, indent=2))


if __name__ == '__main__':
    main()
