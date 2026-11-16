#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""Reproduce a late virtual IRQ at Guest entry using QEMU/GDB, never real hardware."""
import argparse
import json
from pathlib import Path
import socket
import subprocess
import time
from build_guest_bundle import build_bundle

REPO = Path(__file__).resolve().parents[2]


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--firmware-dir', type=Path, required=True)
    ap.add_argument('--output-dir', type=Path, required=True)
    ap.add_argument('--expect', choices=['lost', 'delivered'], required=True)
    ap.add_argument('--no-request', action='store_true', help='Seed PLIC state with the notification bit clear')
    ap.add_argument('--masked', action='store_true', help='Verify masking, then unmask through Guest MMIO')
    ap.add_argument('--cross-compile', default='riscv64-linux-gnu-')
    args = ap.parse_args()
    out = args.output_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    (out/'result.json').unlink(missing_ok=True)
    ld = out / 'guest.lds'
    ld.write_text('OUTPUT_ARCH(riscv)\nENTRY(_image)\nSECTIONS { . = 0xa0200000; .text : { *(.text*) } }\n')
    elf = out / 'guest.elf'
    image = out / 'Image'
    subprocess.run([args.cross_compile+'gcc', '-nostdlib', '-static', '-march=rv64gc',
                    '-mabi=lp64d', '-mcmodel=medany', '-Wl,--build-id=none', '-Wl,-T,'+str(ld),
                    str(REPO/'tests/riscv/vplic-entry-race.s'), '-o', str(elf)], check=True)
    subprocess.run([args.cross_compile+'objcopy', '-O', 'binary', str(elf), str(image)], check=True)
    symbols = {}
    for line in subprocess.check_output([args.cross_compile+'nm', str(elf)], text=True).splitlines():
        fields = line.split()
        if len(fields) == 3:
            symbols[fields[2]] = int(fields[0], 16)
    payload = out / 'SG2044.fd'
    build_bundle([image]*2, [None]*2, ['entry-race-fixture']*2, payload)
    unused = out/'unused.bin'
    unused.write_bytes(bytes.fromhex('6f000000'))
    with socket.socket() as s:
        s.bind(('127.0.0.1', 0))
        port = s.getsockname()[1]
    command = ['qemu-system-riscv64', '-M', 'virt', '-cpu', 'rv64,h=false,zbb=true,sv57=false',
               '-smp', '2', '-m', '4G', '-nographic', '-monitor', 'none', '-S',
               '-gdb', f'tcp:127.0.0.1:{port}', '-bios', str(args.firmware_dir.resolve()/'fw_dynamic.bin'),
               '-kernel', str(unused), '-device', f'loader,file={payload},addr=0x80200000,force-raw=on']
    gdb = out/'test.gdb'
    request = '$v->arch.pending_req & ~4' if args.no_request else '$v->arch.pending_req | 4'
    mask = 'set $p->enable[1][1] = $p->enable[1][1] & ~0x8000' if args.masked else ''
    unmask = f'''if $pc == {symbols['guest_wait']:#x} && ($mip & 0x200) == 0
  printf "MASK_GUARD_OK\\n"
  set $pc = {symbols['guest_unmask']:#x}
  continue
else
  printf "MASK_GUARD_BAD\\n"
end
''' if args.masked else ''
    gdb.write_text(f'''set pagination off
set confirm off
set architecture riscv:rv64
target remote 127.0.0.1:{port}
hbreak load_vmcs if ((struct acrn_vcpu *)$a0)->vm->vm_id == 0 && ((struct acrn_vcpu *)$a0)->arch.contexts[0].run_ctx.cpu_gp_regs.regs.a2 == 0x52414345
continue
set $v = (struct acrn_vcpu *)$a0
set $p = &$v->vm->vplic.regs
printf "ENTRY_WINDOW priority=%u enable=%x\\n", $p->source_priority[47], $p->enable[1][1]
set $p->pending[1] = $p->pending[1] | 0x8000
set $v->arch.pending_req = {request}
set $v->arch.contexts[0].run_ctx.sip = $v->arch.contexts[0].run_ctx.sip & ~0x200
set $mip = ($mip & ~0x200) | 2
{mask}
# Isolate the race: no unrelated monitor timer/IPI can rescue a missed IRQ.
set $mie = $mie & ~0x88
delete breakpoints
hbreak *{symbols['guest_wait']:#x}
hbreak *{symbols['irq_handler']:#x}
continue
{unmask}
printf "FIRST_STOP pc=%lx mip=%lx pending=%x claimed=%x req=%lx\\n", $pc, $mip, $p->pending[1], $p->claimed[1], $v->arch.pending_req
if $pc == {symbols['irq_handler']:#x}
  printf "IRQ_BEFORE_GUEST_WAIT\\n"
  delete breakpoints
  hbreak *{symbols['guest_done']:#x}
  continue
  printf "GUEST_HANDLER_DONE pending=%x claimed=%x mip=%lx\\n", $p->pending[1], $p->claimed[1], $mip
  printf "SSIP_PRESERVED=%u\\n", ($mip & 2) != 0
else
  printf "LOST_PENDING_AT_ENTRY\\n"
end
detach
quit
''')
    with (out/'boot.log').open('w') as log:
        proc = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT)
        try:
            deadline = time.monotonic()+5
            while not any(line.split()[1].endswith(':'+f'{port:04X}') and line.split()[3]=='0A'
                          for line in Path('/proc/net/tcp').read_text().splitlines()[1:]):
                if proc.poll() is not None or time.monotonic()>deadline:
                    raise RuntimeError('QEMU GDB port did not start')
                time.sleep(.03)
            try:
                result = subprocess.run(['gdb-multiarch', '-q', '-nx', '-batch',
                                         str(args.firmware_dir.resolve()/'acrn.elf'), '-x', str(gdb)],
                                        capture_output=True, text=True, timeout=55)
            except subprocess.TimeoutExpired as exc:
                decode = lambda value: value.decode(errors='replace') if isinstance(value, bytes) else value or ''
                result = subprocess.CompletedProcess(exc.cmd, 124, decode(exc.stdout), decode(exc.stderr))
            (out/'gdb.log').write_text(result.stdout+result.stderr)
        finally:
            if proc.poll() is None:
                proc.terminate()
            proc.wait(timeout=5)
    boot = (out/'boot.log').read_text(errors='replace')
    text = (out/'gdb.log').read_text(errors='replace')
    delivered = ('IRQ_BEFORE_GUEST_WAIT' in text and 'IRQ_HANDLED_OK' in boot and
                 'GUEST_HANDLER_DONE pending=0 claimed=0' in text and 'SSIP_PRESERVED=1' in text)
    lost = 'LOST_PENDING_AT_ENTRY' in text and 'IRQ_HANDLED_OK' not in boot
    mask_ok = not args.masked or ('MASK_GUARD_OK' in text and 'MASK_GUARD_BAD' not in text)
    passed = result.returncode == 0 and mask_ok and (delivered if args.expect=='delivered' else lost)
    record = dict(passed=passed, expected=args.expect, delivered=delivered, lost=lost,
                  command=command, debugger_exit=result.returncode,
                  notification_bit_seeded=not args.no_request, masked_then_unmasked=args.masked,
                  ssip_preserved='SSIP_PRESERVED=1' in text,
                  scope='QEMU only; GDB seeds virtual IRQ47 after pending-request processing, before load_vmcs. Monitor timer/IPI masked to prevent unrelated recovery. No physical hardware accessed.')
    (out/'result.json').write_text(json.dumps(record, indent=2)+'\n')
    print(json.dumps(record, indent=2))
    print(text[-3500:])
    raise SystemExit(0 if passed else 1)


if __name__ == '__main__':
    main()
