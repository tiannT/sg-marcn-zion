#!/usr/bin/env python3
"""Use the supplied reference full Asterinas userland and dependencies as direct CPIO files."""
import argparse
import gzip
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
args = parser.parse_args()
args.out_dir.mkdir(parents=True, exist_ok=True)
reference = args.reference
spec = importlib.util.spec_from_file_location('newc', root/'scripts/prepare-asterinas-initramfs.py')
newc = importlib.util.module_from_spec(spec)
spec.loader.exec_module(newc)
sha = lambda b: hashlib.sha256(b).hexdigest()
blob = reference.read_bytes()
public_key = args.authorized_key.read_bytes()
assert public_key.startswith((b'ssh-',b'ecdsa-')), 'Expected an OpenSSH public key'
old = newc.entries(gzip.decompress(blob))
by_name = {e[0]: e for e in old}
programs = ['usr/bin/busybox', 'benchmark/bin/sqlite-speedtest1', 'usr/sbin/mysqld',
            'usr/bin/mysql', 'usr/bin/mysqladmin', 'usr/bin/mysqlslap',
            'usr/local/redis/bin/redis-server', 'usr/local/redis/bin/redis-cli',
            'usr/local/redis/bin/redis-benchmark', 'usr/sbin/dropbear']
assert all(n in by_name for n in programs)
checks = ''.join(sha(by_name[n][3])+'  /'+n+'\n' for n in programs).encode()
init = b'''#!/bin/sh
set -e
mkdir -p /tmp
chmod 1777 /tmp
echo '[PLANB_EMBEDDED_APPS] VERIFY_FILES'
sha256sum -c /etc/planb-reference-files.sha256 > /tmp/planb-reference-files-check.log
/usr/bin/busybox --help 2>&1 | head -n 1
echo '[PLANB_EMBEDDED_APPS] READY redis=7.0.15 mysql=8.0.46 sqlite=3.46.1'
exec /etc/zion-ssh.sh
'''
updates = {
    '.': (stat.S_IFDIR | 0o755, b''),
    '.ssh': (stat.S_IFDIR | 0o700, b''),
    '.ssh/authorized_keys': (stat.S_IFREG | 0o600, public_key),
    'init-planb-asterinas': (stat.S_IFREG | 0o755, init),
    'etc/planb-reference-files.sha256': (stat.S_IFREG | 0o644, checks),
}
for name in list(updates):
    parent = Path(name).parent
    while str(parent) != '.':
        if parent.as_posix() not in by_name:
            updates.setdefault(parent.as_posix(), (stat.S_IFDIR | 0o755, b''))
        parent = parent.parent
pending = dict(updates)
data = bytearray()
unchanged = {}
ino = max(e[2][0] for e in old) + 1
for name, raw_name, fields, body, raw in old:
    if name not in updates:
        data.extend(raw)
        unchanged[name] = sha(raw)
        continue
    mode, body = pending.pop(name)
    fields = list(fields)
    fields[1], fields[2], fields[3], fields[4], fields[5] = mode, 0, 0, 2 if stat.S_ISDIR(mode) else 1, 0
    data.extend(newc.record(raw_name, fields, body))
for name, (mode, body) in sorted(pending.items()):
    data.extend(newc.record(name.encode(), [ino, mode, 0, 0, 2 if stat.S_ISDIR(mode) else 1, 0, 0, 0, 0, 0, 0, 0, 0], body))
    ino += 1
data.extend(newc.record(b'TRAILER!!!', [ino, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0], b''))
data.extend(b'\0' * (-len(data) % 512))
actual = {e[0]: e for e in newc.entries(data)}
assert all(sha(actual[n][4]) == h for n, h in unchanged.items())
assert all(actual[n][3] == by_name[n][3] for n in programs)
assert not any(n.endswith(('apps.tar.xz', 'tee-crypto', 'tee-vault')) for n in actual)
out = args.out_dir/'asterinas-matched.cpio.gz'
assert not out.exists()
out.write_bytes(gzip.compress(bytes(data), compresslevel=9, mtime=0))
out.chmod(0o600)
manifest = dict(reference_sha256=sha(blob), output_sha256=sha(out.read_bytes()),
                output_bytes=out.stat().st_size, unchanged_reference_entries=len(unchanged),
                reference_programs_byte_identical={n:sha(actual[n][3]) for n in programs},
                original_reference_ssh_script_unchanged=True,
                application_unpack_pipeline_present=False,
                updates=sorted(updates), tested=False)
(args.out_dir/'asterinas-manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')
print(json.dumps(manifest, indent=2))
