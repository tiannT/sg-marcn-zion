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

    def test_tamper_rearm_is_rejected_and_cancel_does_not_reset_monitor(self):
        with patch.object(self.c, 'warning_count', return_value=(0, '')), contextlib.redirect_stdout(io.StringIO()):
            self.c.tamper('1')
            with self.assertRaises(RuntimeError):
                self.c.tamper('1')
            self.c.tamper('0')
        with patch.object(self.c, 'warning_count', return_value=(1, '')):
            with self.assertRaises(RuntimeError):
                self.c.tamper('1')
        self.assertEqual(load(self.root / 'state/tamper.json')['phase'], 'cancelled')

    def test_consumption_requires_one_actual_injection_and_monitor_warning(self):
        state = {'phase': 'submitted', 'epoch': 4, 'boot_id': boot_id()}
        save(self.root / 'state/tamper.json', state)
        log = self.root / 'runtime/backend-4.log'
        log.write_text('PLANB_BACKEND_READY\n')
        with patch.object(self.c, 'warning_count', return_value=(1, '')):
            self.assertEqual(self.c.tamper_state()['phase'], 'submitted')
        log.write_text('PLANB_INJECTED_S3 sequence=3 value=0x5a494f4e\n')
        with patch.object(self.c, 'warning_count', return_value=(0, '')):
            self.assertEqual(self.c.tamper_state()['phase'], 'submitted')
        with patch.object(self.c, 'warning_count', return_value=(1, '')):
            self.assertEqual(self.c.tamper_state()['phase'], 'consumed')

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
