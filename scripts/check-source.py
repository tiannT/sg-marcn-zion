#!/usr/bin/env python3
"""Verify this source snapshot without building images or contacting a Host."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    result = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024*1024), b''):
            result.update(block)
    return result.hexdigest()


def main():
    manifest = json.loads((ROOT/'SOURCE_MANIFEST.json').read_text())
    errors = []
    uninitialized = []
    for record in manifest['files']:
        path = ROOT/record['path']
        if 'gitlink' in record:
            result = subprocess.run(['git', '-C', str(ROOT), 'ls-files', '--stage', '--', record['path']],
                                    capture_output=True, text=True, check=True)
            columns = result.stdout.split()
            if len(columns) < 2 or columns[:2] != ['160000', record['gitlink']]:
                errors.append(record['path']+': pinned Git submodule entry differs')
            elif (path/'.git').exists():
                head = subprocess.check_output(['git', '-C', str(path), 'rev-parse', 'HEAD'], text=True).strip()
                if head != record['gitlink']:
                    errors.append(record['path']+': checked-out commit differs')
            else:
                uninitialized.append(record['path'])
        elif 'symlink' in record:
            if not path.is_symlink() or os.readlink(path) != record['symlink']:
                errors.append(record['path']+': symlink differs')
            elif not path.exists() or not path.resolve().is_relative_to(ROOT):
                errors.append(record['path']+': invalid symlink target')
        elif not path.is_file() or path.is_symlink():
            errors.append(record['path']+': missing or nonregular file')
        elif path.stat().st_size != record['bytes'] or digest(path) != record['sha256']:
            errors.append(record['path']+': file changed')
    key_headers = [b'-----BEGIN '+name+b' PRIVATE KEY-----' for name in
                   [b'OPENSSH', b'RSA', b'EC', b'DSA']]
    key_headers.append(b'-----BEGIN '+b'PRIVATE KEY-----')
    for record in manifest['files']:
        if 'symlink' in record or 'gitlink' in record:
            continue
        path = ROOT/record['path']
        if not path.is_file():
            continue
        data = path.read_bytes()
        if any(header in data for header in key_headers):
            errors.append(record['path']+': private key material')
        if data.startswith(b'\x7fELF'):
            errors.append(record['path']+': compiled ELF artifact')
    if errors:
        for error in errors[:30]:
            print(error, file=sys.stderr)
        return 1
    print('SOURCE_SNAPSHOT_OK files='+str(len(manifest['files'])))
    print('PARENT_SOURCE_ONLY_NO_PRIVATE_KEYS_OR_COMPILED_ELF')
    for path in uninitialized:
        print('SUBMODULE_NOT_INITIALIZED '+path+'; run bash scripts/init-submodules.sh before building Linux')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
