"""Root-owned runtime state and SSH connection metadata."""
import json
import os
from pathlib import Path
import subprocess
import tempfile


def atomic(path, data, mode=0o600):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
    with tempfile.NamedTemporaryFile(dir=path.parent, prefix='.' + path.name, delete=False) as f:
        f.write(data.encode() if isinstance(data, str) else data)
        name = f.name
    os.chmod(name, mode)
    os.replace(name, path)


def load(path, default=None):
    try:
        return json.loads(Path(path).read_text())
    except FileNotFoundError:
        return default


def save(path, value):
    atomic(path, json.dumps(value, ensure_ascii=False, indent=2) + '\n')


def boot_id():
    return Path('/proc/sys/kernel/random/boot_id').read_text().strip()


def process_start(pid):
    try:
        data = Path(f'/proc/{pid}/stat').read_text().rsplit(')', 1)[1].split()
        return int(data[19]) if data[0] != 'Z' else None
    except (FileNotFoundError, ProcessLookupError):
        return None


def proxy_live(root):
    record = load(root / 'state/proxy.json', {})
    pid = record.get('pid', 0)
    if record.get('boot_id') != boot_id() or not pid or process_start(pid) != record.get('start'):
        return False
    argv = Path(f'/proc/{pid}/cmdline').read_bytes().split(b'\0')
    return os.fsencode(root / 'compat/portproxy.py') in argv and os.fsencode(root) in argv


def runtime_status(config):
    result = subprocess.run([config['tools'] + '/planb-guestctl', '--status'],
                            capture_output=True, text=True, timeout=10)
    if result.returncode:
        raise RuntimeError(result.stderr.strip() or 'Runtime ABI/status check failed')
    value = json.loads(result.stdout)
    # EBUSY is a refused stop/start, not loss of a still-running Guest.
    if value.get('error') and not (value.get('state') == 1 and value['error'] == 16):
        raise RuntimeError('Runtime reports error: ' + result.stdout.strip())
    return value


def refresh_known(root, status):
    # The file belongs exclusively to this loopback test endpoint.
    import fcntl
    with (root / 'state/proxy-meta.lock').open('a') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        token = [boot_id(), status['image'], status['epoch']]
        if load(root / 'state/known-epoch.json') != token:
            atomic(root / 'state/localhost.known', b'')
            save(root / 'state/known-epoch.json', token)
