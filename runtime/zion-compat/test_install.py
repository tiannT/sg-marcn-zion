"""Check first installation and custom paths without contacting a board."""
import contextlib
import importlib.util
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile
import tempfile
import unittest
from unittest.mock import patch

import install


@unittest.skipUnless(shutil.which('ssh'), 'OpenSSH client is required')
class PortableInstallation(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='zion installation ')
        self.base = Path(self.temp.name)
        self.root = self.base / 'application prefix'
        self.tools = self.base / 'runtime tools'
        self.key = self.base / 'identity' / 'guest key'
        self.entry = self.base / 'entrypoint'
        self.client = self.base / 'ssh' / 'config'
        self.backup = self.base / 'backups'
        self.tools.mkdir()
        self.key.parent.mkdir()
        self.key.write_text('test identity placeholder; not a real private key\n')
        self.key.chmod(0o600)
        self.originals = {}
        for name in ('planb-guestctl', 'planb-backend', 'macrn-pmp-test',
                     'planb-runtime.sh', 'guest-test-client.pub', 'SHA256SUMS'):
            data = ('fixture for ' + name + '\n').encode()
            (self.tools / name).write_bytes(data)
            self.originals[name] = data
        self.client.parent.mkdir()
        self.client.write_text('Host unrelated.example\n    User deployment\n')
        self.original_client = self.client.read_bytes()
        self.native_status = {'state': 0, 'image': 0, 'error': 0, 'epoch': 0,
                              'clean': 15, 'stops': 0, 'private_bytes': 4294967296}

    def tearDown(self):
        self.temp.cleanup()

    def perform_install(self):
        argv = ['install.py', '--prefix', str(self.root), '--tools-dir', str(self.tools),
                '--key', str(self.key), '--entrypoint', str(self.entry),
                '--ssh-config', str(self.client), '--backup-dir', str(self.backup)]
        run = subprocess.run
        original_stat = Path.stat
        calls = []

        def host_run(args, **kwargs):
            calls.append(args)
            if args[0] == 'ss':
                return subprocess.CompletedProcess(args, 0, 'State Recv-Q Send-Q Local Address:Port\n', '')
            if args[0] == str(self.tools / 'planb-guestctl'):
                return subprocess.CompletedProcess(args, 0, json.dumps(self.native_status), '')
            # ssh -G parses configuration locally; it opens no connection.
            return run(args, **kwargs)

        def owned_key(path, **kwargs):
            value = original_stat(path, **kwargs)
            if path == self.key:
                fields = list(value)
                fields[4] = 0  # Simulate root ownership without requiring root.
                return os.stat_result(fields)
            return value

        with patch.object(sys, 'argv', argv), patch('install.os.geteuid', return_value=0), \
             patch('install.subprocess.run', side_effect=host_run), \
             patch.object(Path, 'stat', owned_key), contextlib.redirect_stdout(io.StringIO()):
            install.main()
        self.assertEqual(calls[1], [str(self.tools / 'planb-guestctl'), '--status'])

    def test_custom_install_preserves_tools_and_excludes_identity_from_backup(self):
        self.perform_install()
        config = json.loads((self.root / 'config.json').read_text())
        self.assertEqual(config['tools'], str(self.tools))
        self.assertEqual(config['key'], str(self.key))
        self.assertEqual(self.entry.resolve(), self.root)
        for name, data in self.originals.items():
            self.assertEqual((self.tools / name).read_bytes(), data)
        backup = Path(config['backup'])
        self.assertEqual((backup / 'original-ssh-config').read_bytes(), self.original_client)
        with tarfile.open(backup / 'original-tools.tar.gz') as archive:
            self.assertEqual({Path(name).name for name in archive.getnames()}, set(self.originals))
            self.assertFalse(any(archive.extractfile(member).read() == self.key.read_bytes()
                                 for member in archive.getmembers()))
        for port, identity_present in ((10022, True), (22, False)):
            result = subprocess.run(['ssh', '-G', '-F', str(self.client), '-p', str(port),
                                     'root@127.0.0.1'], capture_output=True, text=True, check=True)
            self.assertEqual('identityfile ' + str(self.key) in result.stdout, identity_present)

    def test_installed_runtime_reads_its_own_configuration(self):
        self.perform_install()
        spec = importlib.util.spec_from_file_location('installed_zion', self.root / 'compat/zion_runtime.py')
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        self.assertEqual(module.ROOT, self.root)
        runtime = module.Compat(module.ROOT)
        runtime.session(initialize=True)
        result = io.StringIO()
        with patch.object(module.os, 'geteuid', return_value=0), \
             patch.object(module, 'runtime_status', return_value=self.native_status) as status, \
             patch.object(module.Compat, 'warning_count', return_value=(0, '')), \
             contextlib.redirect_stdout(result):
            self.assertEqual(module.main(['status', '--json']), 0)
        self.assertEqual(status.call_args.args[0]['tools'], str(self.tools))
        self.assertEqual(json.loads(result.getvalue())['runtime'], self.native_status)

    def test_existing_entry_is_not_overwritten(self):
        self.root.mkdir()
        marker = self.root / 'keep'
        marker.write_text('existing installation\n')
        argv = ['install.py', '--prefix', str(self.root), '--entrypoint', str(self.entry)]
        with patch.object(sys, 'argv', argv), patch('install.os.geteuid', return_value=0), \
             self.assertRaisesRegex(RuntimeError, 'will not be overwritten'):
            install.main()
        self.assertEqual(marker.read_text(), 'existing installation\n')
        self.assertFalse(self.backup.exists())


if __name__ == '__main__':
    unittest.main()
