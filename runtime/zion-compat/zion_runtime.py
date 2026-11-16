#!/usr/bin/env python3
"""Original Zion test command family backed by the real Plan B runtime."""
import fcntl
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import time
from common import atomic, boot_id, load, process_start, proxy_live, refresh_known, runtime_status, save

ROOT = Path(__file__).resolve().parent.parent
HELP = '''Usage: ./zion-runtime.sh select [linux|asterinas]|init|protect|guest|linux|enclave|asterinas|status|stop
       ./zion-runtime.sh tamper-s3 [0|1|status]
Backend: macrn-planb (SG2044 no-H). Run on the management Host as root.
SSH: ssh -p 10022 root@127.0.0.1 (configure the local Guest identity at installation).
enclave uses the unified Linux CVM image. No QEMU PID or fake /dev/tvm.
0008 uses tamper-s3 instead of the unavailable KVM sysfs parameter.
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
        value = load(self.state / 'tamper.json', {'phase': 'idle', 'boot_id': boot_id()})
        if value.get('boot_id') != boot_id():
            value = {'phase': 'idle', 'boot_id': boot_id()}
        if value['phase'] == 'submitted':
            log = Path(self.config.get('runtime_dir', '/run/planb-runtime')) / f"backend-{value['epoch']}.log"
            content = log.read_text() if log.is_file() else ''
            if len(re.findall(r'^PLANB_INJECTED_S3 ', content, re.M)) == 1 and self.warning_count()[0] == 1:
                value['phase'] = 'consumed'
                save(self.state / 'tamper.json', value)
        return value

    def ensure_proxy(self):
        if proxy_live(self.root):
            return
        log = self.logdir / f'ssh-proxy-{time.time_ns()}.log'
        with log.open('ab') as output:
            process = subprocess.Popen([sys.executable, str(self.root / 'compat/portproxy.py'), str(self.root)],
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
        address = {1: '10.0.2.15', 2: '10.0.3.15'}.get(value['image'])
        if value['state'] != 1 or address is None:
            raise RuntimeError('Guest is not running')
        argv = ['ssh', '-i', self.config['key'], '-o', 'IdentitiesOnly=yes',
                '-o', 'IdentityAgent=none', '-o', 'ForwardAgent=no', '-o', 'BatchMode=yes',
                '-o', 'StrictHostKeyChecking=accept-new', '-o', 'ConnectTimeout=3',
                '-o', f'UserKnownHostsFile={self.state}/direct-{value["epoch"]}.known',
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
        target = {'asterinas': 1, 'linux': 2}[profile]
        initial = runtime_status(self.config)
        action = 'start' if initial['state'] == 1 and initial['image'] == target and not inject else 'switch'
        env = os.environ.copy()
        env['PLANB_TEST_TAMPER_S3'] = '1' if inject else '0'
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
            raise RuntimeError('Runtime tool hashes do not match')
        runtime_status(self.config)
        abi = Path('/proc/device-tree/chosen/macrn,pmp-live-test-abi').read_bytes()
        if abi != b'\0\0\0\2':
            raise RuntimeError('PMP fixture ABI mismatch')
        atomic(self.state / 'protected-addr', self.config['pmp_fixture'] + '\n')
        self.ensure_proxy()
        print('[ZION INIT] READY backend=macrn-planb; real runtime ABI checked; no /dev/tvm used')
        print('HOST_BOOT_ID=' + boot_id())
        print('LOG_DIR=' + str(self.logdir))

    def protect(self):
        initial = runtime_status(self.config)
        if self.tamper_state()['phase'] in ('armed', 'submitted'):
            raise RuntimeError('0008 injection is pending; finish/cancel it before protect')
        restore = initial['state'] == 1 and initial['image'] == 1
        try:
            print('[ZION MEMORY] Test profile=linux; fixed bidirectional PMP fixtures')
            self.start('linux')
            result = self.tool([self.tools / 'macrn-pmp-test', '--run'])
            required = ['SIX_CASES_PASSED', 'PRIVATE_CANARIES_OK', 'LINUX_CONTEXT_RESTORED',
                        'PEER_MONITOR_DELTAS=1,1,1']
            address = re.search(r'peer_fixture=([0-9a-fA-F]+)', result.stdout)
            if not all(text in result.stdout for text in required) or not address or \
                    int(address[1], 16) != int(self.config['pmp_fixture'], 16):
                raise RuntimeError('Incomplete real PMP evidence or fixture address mismatch')
            atomic(self.state / 'protected-addr', hex(int(address[1], 16)) + '\n')
            print('[ZION MEMORY] PASS: host/peer accesses blocked and recovered; private canaries retained')
        finally:
            if restore:
                print('[ZION COMPAT] Restoring the prior Asterinas profile; VM1 RAM is reloaded')
                self.start('asterinas')

    def status(self, json_only=False):
        value = runtime_status(self.config)
        warnings, monitor = self.warning_count()
        report = dict(backend='macrn-planb', host_boot_id=boot_id(), runtime=value,
                      warnings=warnings, tamper=self.tamper_state(), ssh='127.0.0.1:10022',
                      proxy_running=proxy_live(self.root), protected_addr=self.config['pmp_fixture'])
        if json_only:
            print(json.dumps(report, ensure_ascii=False))
            return
        print(json.dumps(report, ensure_ascii=False, indent=2))
        print(monitor, end='')
        backend_log = Path('/run/planb-runtime') / f'backend-{value["epoch"]}.log'
        if backend_log.is_file():
            print('Backend log: ' + str(backend_log))
            print('\n'.join(backend_log.read_text().splitlines()[-20:]))
        print('Console evidence: physical serial. The following is an actual SSH status query:')
        if value['state'] == 1:
            refresh_known(self.root, value)
            r = self.ssh('export PATH=/usr/sbin:/usr/bin:/sbin:/bin; uname -a; cat /proc/version; '
                         'ifconfig eth0; if test -b /dev/vda; then echo VIRTIO_BLOCK_PRESENT; fi')
            print(r.stdout, end='')
            if r.returncode:
                print('GUEST_SSH_NOT_READY=' + r.stderr.strip())

    def tamper(self, argument):
        value = self.tamper_state()
        if argument == '1':
            if self.warning_count()[0] != 0 or value['phase'] not in ('idle', 'cancelled'):
                raise RuntimeError('First-alert window already used or an injection is pending; not rearmed')
            value = {'phase': 'armed', 'boot_id': boot_id()}
            save(self.state / 'tamper.json', value)
        elif argument == '0':
            if value['phase'] == 'submitted':
                raise RuntimeError('Injection already submitted to a running backend; cannot cancel it here')
            value = {'phase': 'cancelled', 'boot_id': boot_id()}
            save(self.state / 'tamper.json', value)
        elif argument != 'status':
            raise RuntimeError('Usage: ./zion-runtime.sh tamper-s3 [0|1|status]')
        print('zion_vcpu_tamper=' + ('1' if value['phase'] in ('armed', 'submitted') else '0'))
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
            profile = 'asterinas' if command == 'asterinas' else 'linux'
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
