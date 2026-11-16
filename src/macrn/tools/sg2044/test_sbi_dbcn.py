#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""Test real DBCN calls and hostile physical ranges on both no-H Guests."""
import argparse
import json
from pathlib import Path
import struct
import subprocess
import time
from build_guest_bundle import build_bundle

REPO = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--firmware', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, required=True)
    parser.add_argument('--cross-compile', default='riscv64-linux-gnu-')
    parser.add_argument('--fixture', choices=['dbcn', 'mmio'], default='dbcn')
    parser.add_argument('--no-sstc', action='store_true')
    args = parser.parse_args()
    out = args.output_dir.resolve(); out.mkdir(parents=True, exist_ok=True)
    images = []
    bases, sizes = [0xa0000000, 0x140000000], [0x80000000, 0x40000000]
    for vm in range(2):
        linker = out/f'guest{vm}.lds'
        linker.write_text('OUTPUT_ARCH(riscv)\nENTRY(_start)\nSECTIONS { '
                          f'. = {bases[vm]+0x200040:#x}; .text : {{ *(.text*) }} '
                          '.rodata : { *(.rodata*) } .data : { *(.data*) } }\n')
        elf, binary = out/f'guest{vm}.elf', out/f'guest{vm}.bin'
        command = [args.cross_compile+'gcc', '-nostdlib', '-static', '-fno-pie', '-no-pie',
                   '-march=rv64gc', '-mabi=lp64d', '-mcmodel=medany',
                   '-x', 'assembler-with-cpp',
                   f'-DSTATIC_GUEST_ID={vm}', f'-DOWN_BASE={bases[vm]:#x}',
                   f'-DOWN_SIZE={sizes[vm]:#x}', f'-DPEER_BASE={bases[1-vm]:#x}',
                   '-Wl,--build-id=none', '-Wl,--no-relax', '-Wl,-T,'+str(linker),
                   str(REPO/('tests/riscv/dbcn-guest.s' if args.fixture == 'dbcn' else
                             'tests/riscv/mmio-loads-guest.s')), '-o', str(elf)]
        compile_result = subprocess.run(command, capture_output=True, text=True)
        (out/f'compile{vm}.log').write_text(compile_result.stdout+compile_result.stderr)
        compile_result.check_returncode()
        subprocess.run([args.cross_compile+'objcopy', '-O', 'binary', str(elf), str(binary)], check=True)
        body = binary.read_bytes(); header = bytearray(64)
        struct.pack_into('<IIQQQIIQ', header, 0, 0x0400006f, 0, 0x200000,
                         len(body)+64+16384, 0, 2, 0, 0)
        header[48:56], header[56:60] = b'RISCV\0\0\0', b'RSC\x05'
        image = out/f'Image{vm}'; image.write_bytes(header+body); images.append(image)
    payload = out/'SG2044.fd'
    build_bundle(images, [None, None], ['dbcn-regression']*2, payload)
    unused = out/'unused.bin'; unused.write_bytes(bytes.fromhex('6f000000'))
    cpu = 'rv64,h=false,zbb=true,sv57=false,sv48=false'
    if args.no_sstc:
        cpu += ',sstc=false'
    command = ['qemu-system-riscv64', '-M', 'virt', '-cpu',
               cpu, '-smp', '2', '-m', '4G',
               '-nographic', '-monitor', 'none', '-bios', str(args.firmware.resolve()),
               '-kernel', str(unused), '-device',
               f'loader,file={payload},addr=0x80200000,force-raw=on']
    required = [f'DBCN_VM{vm}_{marker}' for vm in range(2)
                for marker in ['BYTE_OK', 'BULK_OK', 'BOUNDS_AND_API_PASS']]
    if args.fixture == 'mmio':
        required = ['MMIO_VM0_ALIVE', 'MMIO_VM1_SIGN_UNSIGNED_COMPRESSED_SP_AND_STORES_PASS']
    logfile = out/'boot.log'; started = time.monotonic()
    with logfile.open('w') as log:
        proc = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT)
        try:
            while time.monotonic()-started < 30:
                text = logfile.read_text(errors='replace')
                if all(x in text for x in required) or '_FAIL' in text or proc.poll() is not None:
                    break
                time.sleep(.05)
        finally:
            if proc.poll() is None: proc.terminate()
            proc.wait(timeout=5)
    text = logfile.read_text(errors='replace')
    record = dict(passed=all(x in text for x in required) and '_FAIL' not in text,
                  missing=[x for x in required if x not in text], command=command,
                  scope='No-H QEMU, real SBI probes/byte/bulk/read calls, peer/monitor/boundary/overflow/upper-XLEN rejection in both Guests.')
    if args.fixture == 'mmio':
        record['scope'] = 'Actual S-mode MMIO instructions: LB/LBU/LH/LHU/LW/LWU/LD, x0, C.LW/C.LD/C.LWSP/C.LDSP, SB/SH/SW/SD/C.SW/C.SD/C.SWSP/C.SDSP. Values checked against real emulated MAC/divisor/scratch registers.'
    (out/'results.json').write_text(json.dumps(record, indent=2)+'\n')
    print(json.dumps(record, indent=2))
    print(text[-2500:])
    raise SystemExit(0 if record['passed'] else 1)


if __name__ == '__main__':
    main()
