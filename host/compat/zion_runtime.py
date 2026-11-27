#!/usr/bin/env python3
"""Original Zion test command family backed by the real Plan B runtime."""
import fcntl
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import time
from common import atomic, boot_id, load, process_start, proxy_live, refresh_known, runtime_status, save

ROOT = Path(__file__).resolve().parents[1]
TAMPER_PARAMETER = Path('/sys/module/kvm/parameters/zion_vcpu_tamper')
HELP = '''Usage: ./zion-runtime.sh select [linux|asterinas]|init|protect|guest|linux|enclave|asterinas|status|stop
       ./zion-runtime.sh tamper-s3 [0|1|status|preflight]
Backend: SG2044 native ABI1. Run commands locally on the management Host.
SSH: ssh -p 10022 root@127.0.0.1 (existing local test identity configured).
enclave starts Linux in a separate mode without virtio-blk.
0007 uses the real /dev/tvm driver; 0008 accepts the original KVM parameter path.
'''


class Compat:
    def __init__(self, root):
        self.root = root
        self.state = root / 'state'
        self.config = load(root / 'config.json')
        self.tools = Path(self.config['tools'])
        self.logdir = None

    def tool(self, argv, timeout=150, required=True):
        result = subprocess.run([str(a) for a in argv], stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, text=True, timeout=timeout,
                                stdin=subprocess.DEVNULL)
        print(result.stdout, end='', flush=True)
        if self.logdir:
            with (self.logdir / 'commands.log').open('a') as out:
                out.write(time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()) + ' ' + str(argv[0]) + '\n')
                out.write(result.stdout)
        if required and result.returncode:
            raise RuntimeError(f'Command failed with exit={result.returncode}: {argv[0]}')
        return result

    def session(self, initialize=False):
        current = boot_id()
        record = load(self.state / 'session.json', {})
        if record.get('boot_id') != current:
            if not initialize:
                raise RuntimeError('Run ./zion-runtime.sh init for this Host boot')
            self.logdir = self.root / 'logs' / (time.strftime('%Y%m%dT%H%M%SZ', time.gmtime()) + '-' + current[:8])
            self.logdir.mkdir(parents=True, mode=0o700, exist_ok=True)
            record = {'boot_id': current, 'logdir': str(self.logdir)}
            save(self.state / 'session.json', record)
            save(self.state / 'tamper.json', {'phase': 'idle', 'boot_id': current})
        self.logdir = Path(record['logdir'])
        return record

    def warning_count(self):
        r = subprocess.run([str(self.tools / 'planb-backend'), '--status'],
                           capture_output=True, text=True, timeout=10)
        if r.returncode:
            raise RuntimeError(r.stderr.strip() or 'Cannot read monitor warning counter')
        match = re.search(r'\bwarnings=(\d+)\b', r.stdout)
        if not match:
            raise RuntimeError('Monitor warning counter missing')
        return int(match[1]), r.stdout

    def tamper_state(self):
        info = self.native_info()
        value = dict(phase='consumed' if info['used'] else 'armed' if info['tamper'] else 'idle',
                     boot_id=boot_id(), parameter=info['tamper'])
        save(self.state / 'tamper.json', value)
        return value

    def native_info(self):
        r = subprocess.run([str(self.tools/'tvm-control'),'--info'],capture_output=True,text=True,timeout=10)
        if r.returncode:
            raise RuntimeError('Native tvm driver unavailable: '+r.stderr.strip())
        value=json.loads(r.stdout)
        if value.get('abi') != 1:raise RuntimeError('Native tvm compatibility ABI mismatch')
        return value

    def ensure_proxy(self):
        if proxy_live(self.root):
            return
        log = self.logdir / f'ssh-proxy-{time.time_ns()}.log'
        with log.open('ab') as output:
            process = subprocess.Popen([self.config.get('python',sys.executable), str(self.root / 'compat/portproxy.py'), str(self.root)],
                                       stdin=subprocess.DEVNULL, stdout=output, stderr=output,
                                       start_new_session=True)
        save(self.state / 'proxy.json', {'pid': process.pid, 'start': process_start(process.pid), 'boot_id': boot_id()})
        for _ in range(50):
            if process.poll() is not None:
                raise RuntimeError('Cannot bind 127.0.0.1:10022; see ' + str(log))
            if 'ZION_COMPAT_PROXY_READY' in log.read_text():
                return
            time.sleep(.1)
        raise RuntimeError('SSH proxy readiness timeout')

    def ssh(self, command, timeout=12):
        value = runtime_status(self.config)
        address = {1: '10.0.2.15', 2: '10.0.2.15', 3: '10.0.2.15'}.get(value['image'])
        if value['state'] != 1 or address is None:
            raise RuntimeError('Guest is not running')
        argv = ['ssh', '-i', self.config['key'], '-o', 'IdentitiesOnly=yes',
                '-o', 'IdentityAgent=none', '-o', 'ForwardAgent=no', '-o', 'BatchMode=yes',
                '-o', 'StrictHostKeyChecking=accept-new', '-o', 'ConnectTimeout=3',
                '-o', f'UserKnownHostsFile={self.state}/direct-{boot_id()}-{value["epoch"]}.known',
                'root@' + address, command]
        return subprocess.run(argv, stdin=subprocess.DEVNULL, capture_output=True, text=True, timeout=timeout)

    def wait(self):
        last = ''
        for _ in range(30):
            try:
                r = self.ssh('printf GUEST_SSH_READY', timeout=6)
            except subprocess.TimeoutExpired:
                last = 'SSH handshake timeout'
                continue
            if not r.returncode and 'GUEST_SSH_READY' in r.stdout:
                print('GUEST_SSH_READY', flush=True)
                return
            last = r.stderr.strip()
            time.sleep(1)
        raise RuntimeError('Guest SSH did not become ready: ' + last)

    def start(self, profile, inject=False):
        target = {'asterinas': 1, 'linux': 2, 'enclave': 3}[profile]
        initial = runtime_status(self.config)
        action = 'start' if initial['state'] == 1 and initial['image'] == target and not inject else 'switch'
        env = os.environ.copy()
        env['PLANB_TEST_TAMPER_S3'] = '1' if inject else '0'
        env['PLANB_BACKEND_LOG_DIR'] = str(self.logdir)
        # The existing controller checks live Enclaves, serializes stop/clean/reload,
        # and validates backend PID/start-time/boot-ID before terminating anything.
        r = subprocess.run([str(self.tools / 'planb-runtime.sh'), action, profile],
                           stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                           stderr=subprocess.STDOUT, text=True, env=env, timeout=150)
        print(r.stdout, end='', flush=True)
        with (self.logdir / 'commands.log').open('a') as log:
            log.write(r.stdout)
        if r.returncode:
            raise RuntimeError('Runtime start/switch refused; Guest was not forced to stop')
        value = runtime_status(self.config)
        if inject:
            save(self.state / 'tamper.json', {'phase': 'submitted', 'boot_id': boot_id(), 'epoch': value['epoch']})
        refresh_known(self.root, value)
        self.ensure_proxy()
        self.wait()
        atomic(self.state / 'guest-system', profile + '\n')
        print(f'[ZION COMPAT] Guest ready: profile={profile} epoch={value["epoch"]} ssh=127.0.0.1:10022')

    def init(self):
        self.session(initialize=True)
        key = Path(self.config['key'])
        if key.is_symlink() or key.stat().st_uid != 0 or key.stat().st_mode & 0o777 != 0o600:
            raise RuntimeError('Local Guest test key must be root-owned mode 0600')
        r = subprocess.run(['sha256sum', '-c', 'SHA256SUMS'], cwd=self.tools, capture_output=True, text=True)
        print(r.stdout, end='')
        if r.returncode:
            raise RuntimeError('Host tool checksums do not match')
        runtime_status(self.config)
        if not Path('/dev/tvm').exists():
            self.tool(['insmod',self.tools/'tvm-driver.ko'])
        info=self.native_info()
        parameter=TAMPER_PARAMETER
        if not parameter.exists():
            if Path('/sys/module/kvm').exists():
                raise RuntimeError('Existing KVM module has no Zion parameter; no module was replaced')
            self.tool(['insmod',self.tools/'kvm.ko'])
        abi = Path('/proc/device-tree/chosen/macrn,pmp-live-test-abi').read_bytes()
        if abi != b'\0\0\0\2':
            raise RuntimeError('PMP fixture ABI mismatch')
        atomic(self.state / 'protected-addr', info['protected_addr'] + '\n')
        self.tool([self.tools/'tvm-control','tvm',str(self.config.get('pool_pages',8192))])
        self.ensure_proxy()
        console = subprocess.run([str(self.tools / 'planb-backend'), '--console-info'], capture_output=True, text=True, timeout=10)
        if console.returncode:
            raise RuntimeError(console.stderr.strip() or 'Install matching 2044 console firmware')
        print('[ZION INIT] READY backend=2044; runtime ABI verified')
        print('HOST_BOOT_ID=' + boot_id())
        print('LOG_DIR=' + str(self.logdir))

    def protect(self):
        info=self.native_info()
        address=(self.state/'protected-addr').read_text().strip()
        if int(address,16)!=int(info['protected_addr'],16):
            raise RuntimeError('Protected address differs from monitor-owned fixture')
        before=subprocess.run(['dmesg'],capture_output=True,text=True,check=True).stdout.splitlines()
        result=self.tool([self.tools/'tvm-control','phys_memory_access','0',address],required=False)
        after=subprocess.run(['dmesg'],capture_output=True,text=True,check=True).stdout.splitlines()
        new='\n'.join(after[len(before):])
        if not result.returncode:
            raise RuntimeError('[ZION MEMORY] FAIL: host read returned successfully for '+address)
        if 'protected-memory read blocked:' not in new or self.native_info()['faults']!=info['faults']+1:
            raise RuntimeError('[ZION MEMORY] FAIL: access failed without real recovered PMP-fault evidence')
        print('[ZION MEMORY] PASS: host read of '+address+' was blocked and recovered')
        print('Confirm the matching [SM] TEE security check lines on the physical console.')

    def status(self, json_only=False):
        value = runtime_status(self.config)
        warnings, monitor = self.warning_count()
        report = dict(backend='2044', host_boot_id=boot_id(), runtime=value,
                      warnings=warnings, tamper=self.tamper_state(), ssh='127.0.0.1:10022',
                      proxy_running=proxy_live(self.root), protected_addr=self.config['pmp_fixture'])
        if json_only:
            print(json.dumps(report, ensure_ascii=False))
            return
        transcript = subprocess.run([str(self.tools / 'planb-backend'), '--console'],
                                    stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=30)
        if transcript.returncode:
            raise RuntimeError(transcript.stderr.decode(errors='replace').strip() or 'Guest console capture failed')
        current = runtime_status(self.config)
        if current['epoch'] != value['epoch'] or current['image'] != value['image']:
            raise RuntimeError('Guest reloaded while reading its console; retry status')
        log = self.state / 'cvm.log'
        log.write_bytes(transcript.stdout)
        log.chmod(0o600)
        save(self.state / 'cvm-console.json', dict(host_boot_id=boot_id(), epoch=value['epoch'],
             image=value['image'], bytes=len(transcript.stdout),
             sha256=hashlib.sha256(transcript.stdout).hexdigest(), source='actual VM1 SBI and UART bytes'))
        # Preserve the complete Guest console in arrival order.
        sys.stdout.flush()
        sys.stdout.buffer.write(transcript.stdout)
        sys.stdout.buffer.flush()

    def require_tamper_window(self, value):
        if value['phase'] == 'consumed':
            raise RuntimeError(
                'S3 one-shot test already consumed in this Host boot (used=1). '
                'Restart the Host before a new test; restarting the Guest or '
                'reloading the driver cannot reset this window. No injection was written.')
        if value['phase'] == 'armed':
            raise RuntimeError(
                'S3 injection is already armed. Run linux to consume it, or '
                'tamper-s3 0 to cancel it before starting a new test.')
        if self.warning_count()[0] != 0:
            raise RuntimeError(
                'The monitor has already reported an S3 warning in this Host boot. '
                'Restart the Host before a new one-shot test. No injection was written.')

    def tamper(self, argument):
        value = self.tamper_state()
        if argument in ('1', 'preflight'):
            self.require_tamper_window(value)
        if argument == 'preflight':
            print('S3_TEST_READY: used=0 parameter=0 warnings=0')
            return
        elif argument == '1':
            TAMPER_PARAMETER.write_text('1\n')
        elif argument == '0':
            TAMPER_PARAMETER.write_text('0\n')
        elif argument != 'status':
            raise RuntimeError('Usage: ./zion-runtime.sh tamper-s3 [0|1|status|preflight]')
        value=self.tamper_state()
        print('zion_vcpu_tamper=' + str(value['parameter']))
        print(json.dumps(value, ensure_ascii=False))
        print('warnings=' + str(self.warning_count()[0]))


def main(argv):
    if argv and argv[0] in ('help', '--help', '-h'):
        print(HELP)
        return 0
    if os.geteuid() != 0:
        raise RuntimeError('Run on the SG2044 management Host as root')
    c = Compat(ROOT)
    ROOT.joinpath('state').mkdir(exist_ok=True, mode=0o700)
    with (c.state / 'command.lock').open('a') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        command = argv[0] if argv else 'select'
        if command == 'select':
            if len(argv) > 2:
                raise RuntimeError('Usage: select [linux|asterinas]')
            choice = argv[1] if len(argv) == 2 else input('Select Guest: 1/linux or 2/asterinas: ').strip()
            choice = {'1': 'linux', '2': 'asterinas'}.get(choice, choice)
            if choice not in ('linux', 'asterinas'):
                raise RuntimeError('Invalid Guest selection')
            atomic(c.state / 'guest-system', choice + '\n')
            print('Selected Guest system: ' + choice)
            return 0
        if command == 'init' and len(argv) == 1:
            c.init()
            return 0
        c.session()
        if command == 'guest' and len(argv) == 1:
            selection = c.state / 'guest-system'
            if not selection.is_file():
                raise RuntimeError('Run ./zion-runtime.sh select first')
            command = selection.read_text().strip()
        if command in ('linux', 'enclave', 'asterinas') and len(argv) == 1:
            profile = command
            pending = c.tamper_state()
            if pending['phase'] == 'armed' and profile != 'linux':
                raise RuntimeError('S3 injection is armed for Linux; cancel with tamper-s3 0 first')
            c.start(profile, inject=pending['phase'] == 'armed')
            c.tamper_state()
        elif command == 'protect' and len(argv) == 1:
            c.protect()
        elif command == 'stop' and len(argv) == 1:
            c.tool([c.tools / 'planb-runtime.sh', 'stop'])
            print('[ZION COMPAT] Guest stopped and cleaned by the actual monitor/controller')
        elif command == 'status' and argv[1:] in ([], ['--json']):
            c.status(json_only=argv[1:] == ['--json'])
        elif command == 'tamper-s3' and len(argv) <= 2:
            c.tamper(argv[1] if len(argv) == 2 else 'status')
        else:
            raise RuntimeError(HELP)
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main(sys.argv[1:]))
    except (RuntimeError, OSError, ValueError, subprocess.TimeoutExpired) as exc:
        print('[ZION COMPAT] ERROR: ' + str(exc), file=sys.stderr)
        sys.exit(1)
