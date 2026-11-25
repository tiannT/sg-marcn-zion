import contextlib
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch
from common import boot_id, load, process_start, proxy_live, runtime_status, save
from zion_runtime import Compat


class CommandCompatibility(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        (self.root / 'state').mkdir()
        (self.root / 'runtime').mkdir()
        save(self.root / 'config.json', {'tools': '/existing-tools', 'runtime_dir': str(self.root / 'runtime')})
        self.c = Compat(self.root)
        self.c.logdir = self.root

    def tearDown(self):
        self.temp.cleanup()

    def test_cancel_does_not_clear_consumed_native_window(self):
        parameter = self.root / 'parameter'
        parameter.write_text('0\n')
        info = {'abi':1, 'used':1, 'tamper':0}
        with patch.object(self.c, 'native_info', return_value=info), \
             patch('zion_runtime.TAMPER_PARAMETER', parameter), \
             patch.object(self.c, 'warning_count', return_value=(1, '')), \
             contextlib.redirect_stdout(io.StringIO()):
            self.c.tamper('0')
        self.assertEqual(load(self.root / 'state/tamper.json')['phase'], 'consumed')
        self.assertEqual(parameter.read_text(), '0\n')

    def test_consumption_uses_native_state_without_backend_log(self):
        with patch.object(self.c, 'native_info', return_value={'used':0,'tamper':1}):
            self.assertEqual(self.c.tamper_state()['phase'], 'armed')
        with patch.object(self.c, 'native_info', return_value={'used':1,'tamper':0}):
            self.assertEqual(self.c.tamper_state()['phase'], 'consumed')

    def test_incompatible_native_abi_is_rejected(self):
        result = subprocess.CompletedProcess([], 0, '{"abi":0}', '')
        with patch('zion_runtime.subprocess.run', return_value=result):
            with self.assertRaisesRegex(RuntimeError, 'ABI mismatch'):
                self.c.native_info()

    def test_same_profile_is_not_reloaded_and_ambient_tamper_is_disabled(self):
        status = {'state': 1, 'image': 2, 'epoch': 4}
        completed = subprocess.CompletedProcess([], 0, 'actual backend started\n')
        with patch('zion_runtime.runtime_status', return_value=status), \
             patch('zion_runtime.subprocess.run', return_value=completed) as run, \
             patch('zion_runtime.refresh_known'), patch.object(self.c, 'ensure_proxy'), \
             patch.object(self.c, 'wait'), patch.dict(os.environ, {'PLANB_TEST_TAMPER_S3': '1'}), \
             contextlib.redirect_stdout(io.StringIO()):
            self.c.start('linux')
        self.assertEqual(run.call_args.args[0][-2:], ['start', 'linux'])
        self.assertEqual(run.call_args.kwargs['env']['PLANB_TEST_TAMPER_S3'], '0')

    def test_armed_injection_reloads_existing_linux(self):
        status = {'state': 1, 'image': 2, 'epoch': 4}
        completed = subprocess.CompletedProcess([], 0, 'actual backend started\n')
        with patch('zion_runtime.runtime_status', return_value=status), \
             patch('zion_runtime.subprocess.run', return_value=completed) as run, \
             patch('zion_runtime.refresh_known'), patch.object(self.c, 'ensure_proxy'), \
             patch.object(self.c, 'wait'), contextlib.redirect_stdout(io.StringIO()):
            self.c.start('linux', inject=True)
        self.assertEqual(run.call_args.args[0][-2:], ['switch', 'linux'])
        self.assertEqual(run.call_args.kwargs['env']['PLANB_TEST_TAMPER_S3'], '1')
        self.assertEqual(load(self.root / 'state/tamper.json')['phase'], 'submitted')

    def test_proxy_identity_check_does_not_accept_an_unrelated_process(self):
        save(self.root / 'state/proxy.json', {'pid': os.getpid(), 'start': process_start(os.getpid()), 'boot_id': boot_id()})
        self.assertFalse(proxy_live(self.root))
        self.assertIsNotNone(process_start(os.getpid()))

    def test_busy_stop_does_not_misclassify_a_running_guest_as_failed(self):
        result = subprocess.CompletedProcess([], 0, '{"state":1,"image":2,"epoch":4,"error":16}')
        with patch('common.subprocess.run', return_value=result):
            self.assertEqual(runtime_status({'tools':'/existing-tools'})['state'], 1)

    def test_original_sh_invocation_can_read_help(self):
        here = Path(__file__).resolve().parent
        shutil.copy2(here / 'zion-runtime.sh', self.root / 'zion-runtime.sh')
        (self.root / 'compat').symlink_to(here, target_is_directory=True)
        r = subprocess.run(['sh', str(self.root / 'zion-runtime.sh'), '--help'], capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn('select [linux|asterinas]', r.stdout)
        self.assertIn('tamper-s3', r.stdout)


if __name__ == '__main__':
    unittest.main()
