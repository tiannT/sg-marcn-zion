#!/usr/bin/env python3
"""Back up original files, then install the Host-only command adapter."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile
import time


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def absolute_path(value):
    path = Path(value).expanduser()
    if not path.is_absolute() or any(c in str(path) for c in '\n\r\0%'):
        raise argparse.ArgumentTypeError('Use an absolute path without control characters or percent signs')
    return path


def ssh_quote(path):
    return '"' + str(path).replace('\\', '\\\\').replace('"', '\\"') + '"'


def client_fragment(root, key):
    return '''# Guest identity is scoped to the local Zion test endpoint.
Match host 127.0.0.1 user root exec "test %p = 10022"
    IdentityFile {key}
    IdentitiesOnly yes
    IdentityAgent none
    ForwardAgent no
    BatchMode yes
    StrictHostKeyChecking accept-new
    UserKnownHostsFile {known}
    ConnectTimeout 5
Match all
'''.format(key=ssh_quote(key), known=ssh_quote(root/'state/localhost.known'))


def argument_parser():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prefix', type=absolute_path, default=Path('/opt/sg-marcn-zion'))
    parser.add_argument('--tools-dir', type=absolute_path, default=Path('/opt/sg-marcn-zion-tools'))
    parser.add_argument('--key', type=absolute_path, default=Path('/etc/sg-marcn-zion/guest_ed25519'))
    parser.add_argument('--entrypoint', type=absolute_path, default=Path('/root/zion-tests'))
    parser.add_argument('--ssh-config', type=absolute_path, default=Path('/root/.ssh/config'))
    parser.add_argument('--backup-dir', type=absolute_path, default=Path('/var/backups/sg-marcn-zion'))
    parser.add_argument('--tag', default='source')
    return parser


def main():
    parser = argument_parser()
    args = parser.parse_args()
    if not args.tag.replace('-', '').replace('_', '').isalnum():
        parser.error('Invalid source tag')
    if os.geteuid() != 0:
        raise RuntimeError('Run on the SG2044 management Host as root')
    pkg = Path(__file__).resolve().parent
    root = args.prefix
    link = args.entrypoint
    client = args.ssh_config
    if root.exists() or root.is_symlink() or link.exists() or link.is_symlink():
        raise RuntimeError('An entry already exists; it will not be overwritten')
    if client.is_symlink():
        raise RuntimeError('SSH config is a symlink; refusing replacement')
    check = subprocess.run(['ss', '-ltn', 'sport = :10022'], capture_output=True, text=True, check=True)
    if len(check.stdout.strip().splitlines()) > 1:
        raise RuntimeError('Port 10022 is in use; nothing installed')
    tools = args.tools_dir
    key = args.key
    public_files = ['planb-guestctl', 'planb-backend', 'macrn-pmp-test',
                    'planb-runtime.sh', 'tvm-control', 'tvm-driver.ko', 'kvm.ko',
                    'guest-test-client.pub', 'SHA256SUMS']
    for name in public_files:
        if not (tools/name).is_file():
            raise RuntimeError('Missing runtime input: '+str(tools/name))
    if key.resolve().is_relative_to(tools.resolve()):
        raise RuntimeError('Keep the private Guest identity outside the public tools directory')
    if key.stat().st_uid != 0 or key.stat().st_mode & 0o777 != 0o600 or key.is_symlink():
        raise RuntimeError('Existing local Guest identity must be root-owned mode 0600')
    probe = subprocess.run([str(tools / 'planb-guestctl'), '--status'], check=True, capture_output=True, text=True)
    if json.loads(probe.stdout).get('native_compat_abi') != 1:
        raise RuntimeError('Install matching SG2044 native ABI1 firmware before these tools')
    deployment = args.tag + '-' + time.strftime('%Y%m%dT%H%M%SZ', time.gmtime()) + '-' + str(os.getpid())
    backup = args.backup_dir / ('zion-cli-' + deployment)
    backup.mkdir(parents=True, mode=0o700)
    os.chmod(backup.parent, 0o700)
    # Snapshot only the existing public tools and scripts. Never package the key.
    tracked = [tools/name for name in public_files]
    hashes = {str(p): digest(p) for p in tracked}
    with tarfile.open(backup / 'original-tools.tar.gz', 'w:gz') as tar:
        for p in tracked:
            tar.add(p, arcname=str(p).lstrip('/'), recursive=False)
    # Back up existing Host files; a previous source release is not required.
    config_existed = client.is_file()
    old = client.read_bytes() if config_existed else b''
    mode = client.stat().st_mode & 0o777 if config_existed else 0o600
    if config_existed:
        (backup / 'original-ssh-config').write_bytes(old)
        (backup / 'original-ssh-config').chmod(0o600)
    metadata = {'deployment_id': deployment, 'root': str(root), 'root_entry_existed': False,
                'home_entry_existed': False, 'ssh_config_existed': config_existed,
                'ssh_config_mode': mode, 'original_file_hashes': hashes,
                'host_boot_id': Path('/proc/sys/kernel/random/boot_id').read_text().strip(),
                'key_contents_backed_up': False}
    (backup / 'backup.json').write_text(json.dumps(metadata, indent=2) + '\n')
    # Everything above is complete before creating the new entry or changing config.
    release = root / 'compat'
    release.mkdir(parents=True, mode=0o700)
    os.chmod(root, 0o700)
    (root / 'state').mkdir(mode=0o700)
    (root / 'logs').mkdir(mode=0o700)
    for name in ['zion_runtime.py', 'portproxy.py', 'common.py']:
        shutil.copy2(pkg / name, release / name)
    shutil.copy2(pkg / 'zion-runtime.sh', root / 'zion-runtime.sh')
    (root / 'zion-runtime.sh').chmod(0o700)
    config = dict(deployment_id=deployment, tools=str(tools), key=str(key),
                  pmp_fixture='0x400080000', backup=str(backup), backend='2044', platform='2044', native_compat_abi=1)
    (root / 'config.json').write_text(json.dumps(config, indent=2) + '\n')
    (root / 'config.json').chmod(0o600)
    (root / 'state/localhost.known').touch(mode=0o600)
    (root / 'ssh-client.conf').write_text(client_fragment(root, key))
    included = ('Include ' + ssh_quote(root / 'ssh-client.conf') + '\n').encode() + old
    client.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
    temporary = client.with_name('config.compat.' + str(os.getpid()))
    temporary.write_bytes(included)
    temporary.chmod(0o600)
    metadata['installed_ssh_config_sha256'] = hashlib.sha256(included).hexdigest()
    (backup / 'backup.json').write_text(json.dumps(metadata, indent=2) + '\n')
    for port, expect_key in [(10022, True), (22, False)]:
        result = subprocess.run(['ssh', '-G', '-F', str(temporary), '-p', str(port), 'root@127.0.0.1'],
                                capture_output=True, text=True, check=True)
        present = 'identityfile ' + str(key) in result.stdout
        if present != expect_key:
            raise RuntimeError('SSH configuration scope check failed; original configuration retained')
    temporary.replace(client)
    if link != root:
        link.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
        link.symlink_to(root, target_is_directory=True)
    if any(digest(p) != value for p, value in hashes.items()):
        raise RuntimeError('Original tool changed during installation')
    print('INSTALLED=' + str(link / 'zion-runtime.sh'))
    print('BACKUP_DIR=' + str(backup))
    print('Original tools unchanged. No firmware files or SSH server settings modified.')


if __name__ == '__main__':
    try:
        main()
    except (OSError, RuntimeError, subprocess.SubprocessError) as exc:
        print('[ZION INSTALL] ERROR: '+str(exc), file=sys.stderr)
        sys.exit(1)
