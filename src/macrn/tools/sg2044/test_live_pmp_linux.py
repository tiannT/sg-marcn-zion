#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""Run six live PMP cases through SSH while both real Linux Guests keep running."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import socket
import subprocess
import time


def port():
    with socket.socket() as s:
        s.bind(('127.0.0.1',0))
        return s.getsockname()[1]


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--firmware',type=Path,required=True)
    ap.add_argument('--payload-dir',type=Path,required=True)
    ap.add_argument('--test-disk',type=Path,required=True)
    ap.add_argument('--ssh-key',type=Path,required=True)
    ap.add_argument('--output-dir',type=Path,required=True)
    ap.add_argument('--no-sstc',action='store_true')
    ap.add_argument('--svpbmt',action='store_true')
    ap.add_argument('--expect-guard',action='store_true')
    ap.add_argument('--inject-ssip',action='store_true',help='Force an interrupt during a VM1 microprobe through local GDB')
    args = ap.parse_args()
    out = args.output_dir.resolve();out.mkdir(parents=True,exist_ok=True)
    # An interrupted rerun must not borrow results from an earlier attempt.
    for name in ('results.json', 'ssh.log', 'gdb.log'):
        (out/name).unlink(missing_ok=True)
    firmware_sha = hashlib.sha256(args.firmware.read_bytes()).hexdigest()
    tool_sha = hashlib.sha256((args.payload_dir/'macrn-pmp-test').read_bytes()).hexdigest()
    disk = out/'disposable-nvme.ext4'
    shutil.copy2(args.test_disk,disk)
    # Provision only this disposable filesystem. The SSH fixture intentionally
    # has few coreutils; the target board's original filesystem is never opened.
    commands=out/'provision.debugfs'
    commands.write_text('rm /macrn-pmp-test\nwrite '+str(args.payload_dir.resolve()/'macrn-pmp-test')+
                        ' /macrn-pmp-test\nset_inode_field /macrn-pmp-test mode 0100755\n')
    provision=subprocess.run(['debugfs','-w','-f',str(commands),str(disk)],capture_output=True,text=True)
    (out/'provision.log').write_text(provision.stdout+provision.stderr)
    provision.check_returncode()
    unused = out/'unused.bin';unused.write_bytes(bytes.fromhex('6f000000'))
    sshport,netport = port(),port()
    cpu = 'rv64,h=false,zbb=true,sv57=false'
    if args.no_sstc:cpu+=',sstc=false'
    if args.svpbmt:cpu+=',svpbmt=true'
    command = ['qemu-system-riscv64','-M','virt','-cpu',cpu,'-smp','2','-m','4G',
               '-nographic','-monitor','none','-bios',str(args.firmware.resolve()),
               '-kernel',str(unused),'-device',
               f'loader,file={args.payload_dir.resolve()}/SG2044.fd,addr=0x80200000,force-raw=on',
               '-drive',f'file={disk},if=none,id=testnvme,format=raw',
               '-device','nvme,drive=testnvme,serial=LIVEPMP0001',
               '-netdev',f'socket,id=n0,listen=127.0.0.1:{netport}','-device','e1000,netdev=n0']
    debugger = None
    debug_log = None
    if args.inject_ssip:
        debugport=port()
        command += ['-S','-gdb',f'tcp:127.0.0.1:{debugport}']
        script=out/'inject.gdb'
        script.write_text(f'''set pagination off
set confirm off
target remote 127.0.0.1:{debugport}
hbreak pmp_live_begin if ((struct acrn_vcpu *)$a0)->vm->vm_id == 1
continue
delete breakpoints
hbreak vmx_vmrun if probes[1].active && ((struct acrn_vcpu *)$a0)->vm->vm_id == 1
continue
printf "LIVE_PROBE_IRQ_INJECT pc=%lx mie=%lx mip=%lx\\n", $pc, $mie, $mip
set $mip = $mip | 2
delete breakpoints
detach
quit
''')
    relaycmd = ['/usr/bin/qemu-system-riscv64','-M','none','-nodefaults','-display','none',
                '-monitor','none','-serial','none','-netdev',
                f'user,id=slirp0,hostfwd=tcp:127.0.0.1:{sshport}-:22',
                '-netdev',f'socket,id=bridge0,connect=127.0.0.1:{netport}',
                '-netdev','hubport,id=h0,hubid=0,netdev=slirp0',
                '-netdev','hubport,id=h1,hubid=0,netdev=bridge0']
    ssh = ['ssh','-i',str(args.ssh_key.resolve()),'-o','IdentitiesOnly=yes','-o','BatchMode=yes',
           '-o','StrictHostKeyChecking=no','-o',f'UserKnownHostsFile={out}/known_hosts',
           '-o','ConnectTimeout=3','-p',str(sshport),'root@127.0.0.1']
    remote = '''cat /proc/sys/kernel/random/boot_id
/macrn-pmp-test --run
first=$?
/macrn-pmp-test --run
second=$?
printf 'PROBE_EXIT=%s,%s\n' "$first" "$second"
printf 'SSH_AFTER_PROBE_OK\n'
cat /proc/sys/kernel/random/boot_id
cat /sys/devices/system/cpu/online
/macrn-pmp-test --health-nvme /dev/nvme0n1
cat /proc/interrupts
'''
    ssh_result = None
    ssh_timed_out = False
    started=time.monotonic()
    with (out/'boot.log').open('w') as log,(out/'relay.log').open('w') as rlog:
        proc = subprocess.Popen(command,stdout=log,stderr=subprocess.STDOUT)
        relay = None
        try:
            time.sleep(.3)
            relay=subprocess.Popen(relaycmd,stdout=rlog,stderr=subprocess.STDOUT)
            if args.inject_ssip:
                debug_log=(out/'gdb.log').open('w')
                debugger=subprocess.Popen(['gdb-multiarch','-q','-nx','-batch',
                    str(args.firmware.resolve().with_name('acrn.elf')),'-x',str(script)],
                    stdout=debug_log,stderr=subprocess.STDOUT)
            deadline=time.monotonic()+75
            while time.monotonic()<deadline:
                text=(out/'boot.log').read_text(errors='replace')
                if 'Server listening on' in text:
                    try:
                        ssh_result=subprocess.run([*ssh,remote],capture_output=True,timeout=35)
                    except subprocess.TimeoutExpired as exc:
                        ssh_timed_out = True
                        ssh_result = subprocess.CompletedProcess(
                            [*ssh, remote], 124, exc.stdout or b'', exc.stderr or b'')
                    (out/'ssh.log').write_bytes(ssh_result.stdout+ssh_result.stderr)
                    if ssh_result.returncode==0 or ssh_timed_out:break
                if proc.poll() is not None:break
                time.sleep(.2)
        finally:
            for p in [proc,relay,debugger]:
                if p and p.poll() is None:p.terminate()
            for p in [proc,relay,debugger]:
                if p:p.wait(timeout=5)
            if debug_log:debug_log.close()
    boot=(out/'boot.log').read_text(errors='replace')
    text=(out/'ssh.log').read_text(errors='replace') if (out/'ssh.log').exists() else ''
    import re
    ids=re.findall(r'^[0-9a-f]{8}-[0-9a-f-]{27}$',text,re.M)
    passed=bool(ssh_result and ssh_result.returncode==0 and 'SSH_AFTER_PROBE_OK' in text and
                'NVME_READ_EXIT=0' in text and len(ids)==2 and ids[0]==ids[1] and
                '[VM1] LINUX_INIT_READY' in boot and 'timeout, completion polled' not in boot and
                'Kernel panic' not in boot and 'Oops -' not in boot)
    if args.expect_guard:
        passed=passed and text.count('NEED_B2_FIRMWARE')==2 and 'PROBE_EXIT=2,2' in text
    else:
        passed=passed and text.count('SIX_CASES_PASSED')==2 and 'PROBE_EXIT=0,0' in text
        passed=passed and text.count('PEER_MONITOR_DELTAS=1,1,1 PEER_POSITIVE_CONTROLS=7')==2
        passed=passed and '[PMP-LIVE VM0] POSITIVE_CONTROLS_OK' in text
    if args.inject_ssip:
        debug=(out/'gdb.log').read_text(errors='replace')
        passed=passed and 'LIVE_PROBE_IRQ_INJECT' in debug and bool(re.search(r'VM1\] POSITIVE_CONTROLS_OK IRQ_RETRIES=[1-9]',boot))
    firmware_unchanged = hashlib.sha256(args.firmware.read_bytes()).hexdigest() == firmware_sha
    tool_unchanged = hashlib.sha256((args.payload_dir/'macrn-pmp-test').read_bytes()).hexdigest() == tool_sha
    passed = passed and firmware_unchanged and tool_unchanged
    record=dict(passed=bool(passed),command=command,relay_command=relaycmd,
                ssh_exit=ssh_result.returncode if ssh_result else None,boot_id_unchanged=len(ids)==2 and ids[0]==ids[1],
                batches_passed=text.count('SIX_CASES_PASSED'),guard=args.expect_guard,
                interrupt_injected=args.inject_ssip,
                ssh_timed_out=ssh_timed_out,
                nvme_timeout_poll='timeout, completion polled' in boot,
                vm1_case_counts={name:boot.count(f'[PMP-LIVE VM1] {name} PASS') for name in ['read','write','execute']},
                firmware_sha256=firmware_sha,firmware_unchanged=firmware_unchanged,
                tool_sha256=tool_sha,tool_unchanged=tool_unchanged,
                elapsed_seconds=round(time.monotonic()-started,3),
                scope='Linux-triggered monitor-owned short U-mode probes on no-H QEMU; original Linux contexts are restored. VM0 booted a disposable NVMe root '
                      'and kept SSH; VM1 stayed Linux with a userspace service. No physical board accessed.')
    (out/'results.json').write_text(json.dumps(record,indent=2)+'\n')
    print(json.dumps(record,indent=2));print(text[-4500:]);print(boot[-2500:])
    raise SystemExit(0 if passed else 1)


if __name__=='__main__':
    main()
