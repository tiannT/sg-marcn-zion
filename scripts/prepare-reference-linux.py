#!/usr/bin/env python3
"""Keep the supplied reference complete Linux userland/module; add the native boot adapter."""
import gzip
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import stat

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--out-dir', type=Path, required=True)
parser.add_argument('--reference', type=Path, required=True)
parser.add_argument('--authorized-key', type=Path, required=True)
parser.add_argument('--pmp-helper', type=Path, required=True)
args = parser.parse_args()
args.out_dir.mkdir(parents=True, exist_ok=True)
reference = args.reference
spec = importlib.util.spec_from_file_location('newc', root/'scripts/prepare-asterinas-initramfs.py')
newc = importlib.util.module_from_spec(spec)
spec.loader.exec_module(newc)
sha = lambda b: hashlib.sha256(b).hexdigest()
reference_blob = reference.read_bytes()
public_key = args.authorized_key.read_bytes()
assert public_key.startswith((b'ssh-',b'ecdsa-')), 'Expected an OpenSSH public key'
old = newc.entries(gzip.decompress(reference_blob))
by_name = {e[0]: e for e in old}
adapter = b'''#!/bin/sh
set -e
export PATH=/usr/sbin:/usr/bin:/sbin:/bin
mount -t devtmpfs devtmpfs /dev
mkdir -p /dev/pts /dev/shm /proc /sys /run /tmp
mount -t proc proc /proc
mount -t sysfs sysfs /sys
mount -t tmpfs tmpfs /dev/shm
/usr/bin/macrn-pmp-test --service </dev/null > /tmp/pmp-service.log 2>&1 &
printf '%s\\n' "$!" > /run/pmp-service.pid
exec /etc/zion-ssh.sh
'''
updates = {
    # Dropbear correctly rejects pubkey auth when root's home '/' is group writable.
    '.': (stat.S_IFDIR | 0o755, b''),
    'init-planb': (stat.S_IFREG | 0o755, adapter),
    'usr/bin/macrn-pmp-test': (stat.S_IFREG | 0o755, args.pmp_helper.read_bytes()),
    'root/.ssh/authorized_keys': (stat.S_IFREG | 0o600, public_key),
    'root/.ssh': (stat.S_IFDIR | 0o700, b''),
    # The unmodified reference /etc/passwd defines root's home as '/'.
    '.ssh/authorized_keys': (stat.S_IFREG | 0o600, public_key),
    '.ssh': (stat.S_IFDIR | 0o700, b''),
}
for name in list(updates):
    parent = Path(name).parent
    while str(parent) != '.':
        if parent.as_posix() not in by_name:
            updates.setdefault(parent.as_posix(), (stat.S_IFDIR | 0o755, b''))
        parent = parent.parent
pending = dict(updates)
data = bytearray()
ino = max(e[2][0] for e in old) + 1
unchanged = {}
for name, raw_name, fields, body, raw in old:
    if name not in updates:
        data.extend(raw)
        unchanged[name] = sha(raw)
        continue
    mode, body = pending.pop(name)
    fields = list(fields)
    fields[1], fields[4], fields[5] = mode, 2 if stat.S_ISDIR(mode) else 1, 0
    data.extend(newc.record(raw_name, fields, body))
for name, (mode, body) in sorted(pending.items()):
    data.extend(newc.record(name.encode(), [ino, mode, 0, 0, 2 if stat.S_ISDIR(mode) else 1, 0, 0, 0, 0, 0, 0, 0, 0], body))
    ino += 1
data.extend(newc.record(b'TRAILER!!!', [ino, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0], b''))
data.extend(b'\0' * (-len(data) % 512))
actual = {e[0]: e for e in newc.entries(data)}
assert all(sha(actual[n][4]) == h for n, h in unchanged.items())
programs = [n for n in by_name if n.startswith('root/zion-enclave/') and stat.S_ISREG(by_name[n][2][1])]
assert all(actual[n][3] == by_name[n][3] for n in programs)
out = args.out_dir/'linux-matched.cpio.gz'
assert not out.exists()
out.write_bytes(gzip.compress(bytes(data), compresslevel=9, mtime=0))
out.chmod(0o600)
manifest = dict(reference_sha256=sha(reference_blob), output_sha256=sha(out.read_bytes()),
                output_bytes=out.stat().st_size, reference_entries_unchanged=len(unchanged),
                reference_enclave_programs_and_module_byte_identical=programs,
                native_adapter_paths=sorted(updates), tested=False)
(args.out_dir/'linux-manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')
print(json.dumps(manifest, indent=2))
