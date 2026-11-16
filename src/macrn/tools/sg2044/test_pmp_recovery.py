#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""Verify real PMP denials and both Guests' recovery on a no-H QEMU monitor."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import time

from build_guest_bundle import build_bundle

REPO = Path(__file__).resolve().parents[2]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def build_payload(out, probe_vm, access, target, prefix, board=False,
                  user=False, vectored=False, sie=True, compressed=False, paging=False):
    """Use the same Linux-profile monitor; only the two Guest payloads vary."""
    out.mkdir(parents=True, exist_ok=True)
    bases = [0xa0000000, 0x400000000 if board else 0x140000000]
    address = {'peer': bases[1-probe_vm] + 0x80000 + (8 if access == 'execute' else 0),
               'monitor': 0x90000000, 'loader': 0x80000000,
               'shared': 0x92000000, 'device': 0x30000000}[target]
    images, commands = [], []
    for vm, base in enumerate(bases):
        ld = out/f'guest{vm}.lds'
        ld.write_text('OUTPUT_ARCH(riscv)\nENTRY(_start)\nSECTIONS { '
                      f'. = {base+0x200040:#x}; .text : {{ *(.text*) }} '
                      '.rodata : { *(.rodata*) } .bss : { *(.bss*) } }\n')
        elf, raw = out/f'guest{vm}.elf', out/f'guest{vm}.bin'
        command = [prefix+'gcc', '-nostdlib', '-static', '-fno-pie', '-no-pie', '-march=rv64gc', '-mabi=lp64d',
                   '-mcmodel=medany', '-x', 'assembler-with-cpp', '-D__ASSEMBLY__',
                   f'-DSTATIC_GUEST_ID={vm}', f'-DSTATIC_PROBE_VM={probe_vm}',
                   f'-DPMP_PROBE_KIND={dict(read=5,write=7,execute=1)[access]}',
                   f'-DPMP_TARGET_ADDR={address:#x}', f'-DPMP_PROBE_USER={int(user)}',
                   f'-DPMP_PROBE_VECTORED={int(vectored)}', f'-DPMP_PROBE_SIE={int(sie)}',
                   '-I'+str(REPO/'hypervisor/include/arch/riscv'), '-Wl,--build-id=none',
                   '-Wl,--no-relax', '-Wl,-T,'+str(ld)]
        if board:
            command += ['-DCONFIG_PLATFORM_SG2044', '-DCONFIG_TIMEBASE_HZ=50000000']
        if compressed:
            command.append('-DPMP_PROBE_COMPRESSED')
        if paging:
            command.append('-DPMP_PROBE_PAGING')
        command += [str(REPO/'tests/riscv/pmp-recover-guest.s'), '-o', str(elf)]
        result = subprocess.run(command, capture_output=True, text=True)
        (out/f'compile{vm}.log').write_text(result.stdout+result.stderr)
        result.check_returncode()
        commands.append(command)
        subprocess.run([prefix+'objcopy','-O','binary',str(elf),str(raw)],check=True)
        (out/f'guest{vm}.disassembly.txt').write_text(subprocess.check_output(
            [prefix+'objdump','-d',str(elf)],text=True))
        body = raw.read_bytes()
        header = bytearray(64)
        struct.pack_into('<IIQQQIIQ',header,0,0x0400006f,0,0x200000,len(body)+64+8192,0,2,0,0)
        header[48:56], header[56:60] = b'RISCV\0\0\0', b'RSC\x05'
        image = out/f'Image-vm{vm}'
        image.write_bytes(header+body)
        images.append(image)
    return images, commands, address


def run_case(firmware, out, prefix, case, expect_recovery=True):
    images, commands, address = build_payload(out, prefix=prefix, **case)
    payload = out/'SG2044.fd'
    build_bundle(images,[None,None],['scheme-b-pmp-recovery']*2,payload)
    unused = out/'unused.bin'
    unused.write_bytes(bytes.fromhex('6f000000'))
    vm, access, target = case['probe_vm'], case['access'], case['target']
    if access == 'execute':
        denial = f'VM{vm} DENIED_PRIVATE_EXECUTE'
    elif target in ('monitor','loader'):
        denial = f'Guest attempted monitor-memory access: {address:x}'
    else:
        denial = f'VM{vm} DENIED_'+('DEVICE_ACCESS' if target == 'device' else 'PRIVATE_ACCESS')
    required = [denial]
    for guest in range(2):
        required += [f'VM{guest} HART{guest} PMP_PRIVATE_READBACK_OK']
        required += [f'[RECOVER VM{guest}] {item}' for item in (
            'SBI_BOUNDARY_OK','SHARED_OK','TIMER_IRQ_OK','PRIVATE_CANARY_OK',
            'SHARED_AFTER_DENIAL_OK','ALIVE_AFTER_DENIAL TIMER_TICKS=3')]
    recovery = f'[RECOVER VM{vm}] ACCESS_DENIED_RECOVERED errno=-14'
    results = {}
    for mode,suffix in [('sstc',''),('no-sstc',',sstc=false')]:
        command = ['qemu-system-riscv64','-M','virt','-cpu',
                   'rv64,h=false,zbb=true,sv57=false'+suffix,'-smp','2','-m','4G',
                   '-nographic','-monitor','none','-bios',str(firmware),'-kernel',str(unused),
                   '-device',f'loader,file={payload},addr=0x80200000,force-raw=on']
        logfile = out/f'{mode}.log'
        started = time.monotonic()
        with logfile.open('w') as stream:
            proc = subprocess.Popen(command,stdout=stream,stderr=subprocess.STDOUT)
            try:
                while time.monotonic()-started < 15:
                    text = logfile.read_text(errors='replace')
                    if (not expect_recovery and denial in text
                            or all(x in text for x in required) and text.count(recovery)==3
                            or '] FAIL' in text or proc.poll() is not None):
                        break
                    time.sleep(.04)
            finally:
                if proc.poll() is None:
                    proc.terminate()
                proc.wait(timeout=5)
        text = logfile.read_text(errors='replace')
        recovered = all(x in text for x in required) and text.count(recovery)==3 and '] FAIL' not in text
        reproduced = denial in text and recovery not in text and '] FAIL' not in text
        results[mode] = dict(passed=recovered if expect_recovery else reproduced,
                             recovered=recovered,baseline_stopped_offender=reproduced,
                             missing=[x for x in required if x not in text],
                             recovery_count=text.count(recovery),case=case,command=command,
                             elapsed_seconds=round(time.monotonic()-started,3))
        print(json.dumps(dict(output=out.name,mode=mode,passed=results[mode]['passed'],
                              recovery_count=text.count(recovery),missing=results[mode]['missing'])),flush=True)
    (out/'results.json').write_text(json.dumps(dict(results=results,compile_commands=commands),indent=2)+'\n')
    return results


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--firmware',type=Path,required=True)
    ap.add_argument('--output-dir',type=Path,required=True)
    ap.add_argument('--cross-compile',default='riscv64-linux-gnu-')
    ap.add_argument('--baseline',action='store_true',help='Require the old monitor to deny but fail recovery')
    ap.add_argument('--quick',action='store_true')
    args = ap.parse_args()
    firmware_digest = sha(args.firmware)
    cases = [dict(probe_vm=vm,access=access,target='peer')
             for vm in range(2) for access in ('read','write','execute')]
    if not args.quick and not args.baseline:
        cases += [dict(probe_vm=vm,access=access,target=target)
                  for vm in range(2) for access in ('read','write','execute')
                  for target in ('monitor','loader')]
        cases += [dict(probe_vm=vm,access='execute',target='shared') for vm in range(2)]
        cases += [dict(probe_vm=1,access=access,target='device') for access in ('read','write')]
        for vm in range(2):
            for access in ('read','write','execute'):
                for options in (dict(user=True,vectored=True),dict(sie=False),
                                dict(compressed=True),dict(paging=True,vectored=True)):
                    cases.append(dict(probe_vm=vm,access=access,target='peer',**options))
    results = {}
    for i,case in enumerate(cases):
        label = f'{i:02}-vm{case["probe_vm"]}-{case["target"]}-{case["access"]}'
        results[label] = run_case(args.firmware.resolve(),args.output_dir.resolve()/label,
                                 args.cross_compile,case,not args.baseline)
    record = dict(passed=all(r['passed'] for modes in results.values() for r in modes.values()),
                  firmware_sha256=firmware_digest,firmware_unchanged=sha(args.firmware)==firmware_digest,
                  fixture_sha256=sha(REPO/'tests/riscv/pmp-recover-guest.s'),
                  case_count=2*len(cases),baseline=args.baseline,
                  scope='No-H QEMU, real S/U CPU access faults and Guest fixups; no SG2044 hardware or disk accessed.',
                  results=results)
    (args.output_dir/'results.json').write_text(json.dumps(record,indent=2)+'\n')
    print(json.dumps({key:value for key,value in record.items() if key!='results'},indent=2))
    raise SystemExit(0 if record['passed'] and record['firmware_unchanged'] else 1)


if __name__ == '__main__':
    main()
