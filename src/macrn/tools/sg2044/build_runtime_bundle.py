#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""One loader slot: fixed VM0 plus selectable Asterinas/Linux VM1 images."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import struct

MAX_SIZE = 0x88000000 - 0x80200000
HEADER_SIZE = 192


def build_bundle(kernels, initrds, cmdlines, output, board_dtb, initial_image=1):
    if not (len(kernels) == len(initrds) == len(cmdlines) == 3) or initial_image not in (1, 2):
        raise ValueError('VM0, Asterinas and Linux images are required')
    data = bytearray(HEADER_SIZE)
    chunks, entries, manifest = {}, [], []

    def add(blob):
        digest = hashlib.sha256(blob).hexdigest()
        if digest not in chunks:
            data.extend(b'\0' * (-len(data) % 4096))
            chunks[digest] = len(data)
            data.extend(blob)
        return chunks[digest], len(blob), digest

    for n, role in enumerate(('management-linux', 'asterinas', 'linux-cvm')):
        kernel = Path(kernels[n]).read_bytes()
        if len(kernel) < 64 or kernel[56:60] != b'RSC\x05':
            raise ValueError('Expected an uncompressed RISC-V Image')
        text, size = struct.unpack_from('<QQ', kernel, 8)
        if text != 0x200000 or size > 0xfe00000 or len(kernel) > 0xfe00000:
            raise ValueError('Kernel exceeds fixed layout')
        ko, ks, kh = add(kernel)
        io, iz, ih = add(Path(initrds[n]).read_bytes()) if initrds[n] else (0, 0, None)
        if iz > 0x30000000:
            raise ValueError('Initrd exceeds the smaller supported VM1 partition')
        command = cmdlines[n].encode() + b'\0'
        if len(command) > 4096 or b'\0' in command[:-1]:
            raise ValueError('Invalid command line')
        co, cs, _ = add(command)
        entries.append((ko, ks, io, iz, co, cs))
        manifest.append(dict(image=n, vm=0 if n == 0 else 1, role=role,
                             kernel_sha256=kh, kernel_bytes=ks, image_size=size,
                             initrd_sha256=ih, initrd_bytes=iz, cmdline=cmdlines[n]))
    fdt = Path(board_dtb).read_bytes()
    if len(fdt) < 40 or struct.unpack_from('>II', fdt) != (0xd00dfeed, len(fdt)):
        raise ValueError('Invalid board DTB')
    fo, fs, fh = add(fdt)
    if len(data) > MAX_SIZE:
        raise ValueError(f'Bundle exceeds load slot: {len(data)} > {MAX_SIZE}')
    struct.pack_into('<8sIIQQQ', data, 0, b'MACRNBRT', 2, 3, len(data), fo, fs)
    for n, entry in enumerate(entries):
        struct.pack_into('<6Q', data, 40 + n * 48, *entry)
    struct.pack_into('<II', data, 184, initial_image, 0)
    output = Path(output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(data)
    output.chmod(0o600)  # DTB may contain private single-boot acceptance entropy.
    result = dict(format='MACRNBRT v2', vm_domains=2, payload_images=3,
                  initial_vm1_image=initial_image, same_host_boot_switch=True,
                  load_address='0x80200000', load_end=hex(0x80200000+len(data)),
                  maximum_bytes=MAX_SIZE, bytes=len(data),
                  sha256=hashlib.sha256(data).hexdigest(), images=manifest,
                  board_dtb_sha256=fh, board_dtb_bytes=fs,
                  security='Artifact hashes identify bytes; boot-chain authentication is separate')
    output.with_suffix(output.suffix+'.json').write_text(json.dumps(result, indent=2)+'\n')
    return result


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('vm0', 'asterinas', 'linux'):
        p.add_argument('--'+name+'-kernel', type=Path, required=True)
        p.add_argument('--'+name+'-initrd', type=Path)
        p.add_argument('--'+name+'-cmdline', required=True)
    p.add_argument('--initial', choices=['asterinas','linux'], default='asterinas')
    p.add_argument('--board-dtb', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    names = ('vm0', 'asterinas', 'linux')
    print(json.dumps(build_bundle([getattr(a,n+'_kernel') for n in names],
        [getattr(a,n+'_initrd') for n in names], [getattr(a,n+'_cmdline') for n in names],
        a.output, a.board_dtb, 1 if a.initial == 'asterinas' else 2), indent=2))


if __name__ == '__main__':
    main()
