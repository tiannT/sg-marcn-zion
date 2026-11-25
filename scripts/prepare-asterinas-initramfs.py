#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""Put genuine BusyBox 1.30.1 and the Asterinas init into a fresh newc archive."""
import argparse
import gzip
import hashlib
import json
from pathlib import Path, PurePosixPath
import stat
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def digest(blob):
    return hashlib.sha256(blob).hexdigest()


def entries(blob):
    position = 0
    result, names = [], set()
    while True:
        if blob[position:position + 6] != b'070701':
            raise ValueError('Input must contain a newc archive')
        fields = [int(blob[position + 6 + i * 8:position + 14 + i * 8], 16)
                  for i in range(13)]
        raw_name = blob[position + 110:position + 110 + fields[11] - 1]
        name = str(PurePosixPath(raw_name.decode()))
        start = (position + 110 + fields[11] + 3) & ~3
        end = (start + fields[6] + 3) & ~3
        if end > len(blob):
            raise ValueError('Truncated newc entry')
        if name == 'TRAILER!!!':
            break
        if name.startswith('/') or '..' in PurePosixPath(name).parts or name in names:
            raise ValueError('Unsafe or duplicate archive path: ' + name)
        names.add(name)
        result.append((name, raw_name, fields, blob[start:start + fields[6]],
                       blob[position:end]))
        position = end
    return result


def record(name, fields, body):
    encoded = name + b'\0'
    fields = list(fields)
    fields[6], fields[11] = len(body), len(encoded)
    blob = b'070701' + b''.join(f'{value:08x}'.encode() for value in fields) + encoded
    blob += b'\0' * (-len(blob) % 4)
    blob += body
    return blob + b'\0' * (-len(blob) % 4)


def busybox_version(path, qemu):
    blob = path.read_bytes()
    if blob[:6] != b'\x7fELF\x02\x01' or struct.unpack_from('<H', blob, 18)[0] != 243:
        raise ValueError('BusyBox must be a little-endian RISC-V ELF64 binary')
    offset = struct.unpack_from('<Q', blob, 32)[0]
    size, count = struct.unpack_from('<HH', blob, 54)
    if any(struct.unpack_from('<I', blob, offset + n * size)[0] == 3 for n in range(count)):
        raise ValueError('Provide static BusyBox so the old rootfs loader is not required')
    run = subprocess.run([qemu, str(path), '--help'], check=True, capture_output=True,
                         text=True, timeout=15)
    line = (run.stdout + run.stderr).splitlines()[0]
    if not line.startswith('BusyBox v1.30.1 '):
        raise ValueError('Wrong BusyBox version: ' + line)
    return blob, line


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', type=Path, required=True)
    parser.add_argument('--busybox-binary', type=Path, required=True)
    parser.add_argument('--init-script', type=Path,
                        default=ROOT / 'runtime/guest/asterinas-init.sh')
    parser.add_argument('--qemu-riscv64', default='qemu-riscv64')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    manifest_path = args.output.with_suffix(args.output.suffix + '.json')
    if args.output.exists() or manifest_path.exists():
        parser.error('Choose fresh output paths to preserve previous artifacts')
    binary, version = busybox_version(args.busybox_binary.resolve(), args.qemu_riscv64)
    source = args.input.read_bytes()
    old = entries(gzip.decompress(source))
    if 'usr/bin/busybox' not in {item[0] for item in old}:
        raise ValueError('Expected /usr/bin/busybox in the input rootfs')
    replacements = {
        'usr/bin/busybox': (stat.S_IFREG | 0o755, binary),
        'init-planb-asterinas': (stat.S_IFREG | 0o755, args.init_script.read_bytes()),
        'etc/hostname': (stat.S_IFREG | 0o644, b'zion-cvm\n'),
    }
    pending = dict(replacements)
    unchanged, changed = {}, []
    result = bytearray()
    inode = max(item[2][0] for item in old) + 1
    for name, raw_name, fields, body, raw in old:
        if name not in replacements:
            result.extend(raw)
            unchanged[name] = digest(raw)
            continue
        mode, content = pending.pop(name)
        if fields[4] != 1 or not stat.S_ISREG(fields[1]):
            raise ValueError('Expected a single-link regular file: ' + name)
        fields = list(fields)
        fields[1], fields[5] = mode, 0
        result.extend(record(raw_name, fields, content))
        changed.append(name)
    for name, (mode, body) in sorted(pending.items()):
        fields = [inode, mode, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0]
        result.extend(record(name.encode(), fields, body))
        inode += 1
    result.extend(record(b'TRAILER!!!', [inode, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0], b''))
    result.extend(b'\0' * (-len(result) % 512))
    rebuilt = {item[0]: item for item in entries(result)}
    if not all(digest(rebuilt[name][4]) == value for name, value in unchanged.items()):
        raise RuntimeError('An unrelated original rootfs entry changed')
    if not all(rebuilt[name][3] == value[1] for name, value in replacements.items()):
        raise RuntimeError('Rootfs replacement verification failed')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(gzip.compress(bytes(result), compresslevel=9, mtime=0))
    args.output.chmod(0o600)
    manifest = dict(input_sha256=digest(source), output_sha256=digest(args.output.read_bytes()),
                    output_bytes=args.output.stat().st_size, busybox_version=version,
                    busybox_sha256=digest(binary), hostname='zion-cvm',
                    replaced=changed, added=sorted(pending),
                    untouched_entries_verified=len(unchanged), duplicate_paths=False)
    manifest_path.write_text(json.dumps(manifest, indent=2) + '\n')
    print(json.dumps(manifest, indent=2))


if __name__ == '__main__':
    main()
