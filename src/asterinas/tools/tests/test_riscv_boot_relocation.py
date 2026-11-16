#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""SG2044-P1-BOOT-001: boot the real BSP at multiple physical addresses.

Run in the project build container. This checks the actual bootstrap/MMU
handoff with an assembly substitute for riscv_boot; it is not an OS boot test.
"""

import argparse
import json
from pathlib import Path
import struct
import subprocess
import time


REPO = Path(__file__).resolve().parents[2]

STUB = r'''
.text
.global riscv_boot
.type riscv_boot, @function
riscv_boot:
    la t0, __riscv_kernel_load_offset
    ld t0, 0(t0)
    li t1, 0xffffffff80200000 - EXPECTED_PADDR
    bne t0, t1, stub_fail
    li t1, 0xffffffff80000000
    bltu sp, t1, stub_fail
    auipc t0, 0
    bltu t0, t1, stub_fail
    la t0, __cpu_local_start
    bne gp, t0, stub_fail
    lwu t0, 0(a1)
    li t1, 0xedfe0dd0
    bne t0, t1, stub_fail
    csrr t0, satp
    srli t0, t0, 60
    li t1, EXPECTED_MODE
    bne t0, t1, stub_fail
    la s0, stub_ok
    j stub_print
stub_fail:
    la s0, stub_bad
stub_print:
    lbu a0, 0(s0)
    beqz a0, stub_done
    li a7, 1
    ecall
    addi s0, s0, 1
    j stub_print
stub_done:
    j stub_done
.size riscv_boot, .-riscv_boot
.section .rodata
stub_ok: .asciz "ASTER_BOOT_RELOCATION_OK\n"
stub_bad: .asciz "ASTER_BOOT_RELOCATION_FAIL\n"
'''


def compile_image(compiler, objcopy, folder, source, expected, mode):
    folder.mkdir(parents=True, exist_ok=True)
    (folder/'bsp.S').write_text(source)
    (folder/'stub.S').write_text(STUB)
    linker = REPO/'osdk/src/base_crate/riscv64.ld.template'
    elf = folder/'fixture.elf'
    command = [*compiler, '-nostdlib', '-static', '-fno-pie', '-no-pie',
               '-march=rv64imac', '-mabi=lp64', '-mcmodel=medany',
               f'-DEXPECTED_PADDR={expected:#x}', f'-DEXPECTED_MODE={mode}',
               '-Wl,--no-relax', '-Wl,--build-id=none', '-Wl,-T,'+str(linker),
               str(folder/'bsp.S'), str(folder/'stub.S'), '-o', str(elf)]
    result = subprocess.run(command, capture_output=True, text=True)
    (folder/'compile.log').write_text(result.stdout+result.stderr)
    result.check_returncode()
    image = folder/'Image'
    subprocess.run([objcopy, '-O', 'binary', str(elf), str(image)], check=True)
    data = image.read_bytes()
    assert len(data) > 64 and data[48:60] == b'RISCV\0\0\0RSC\x05'
    assert struct.unpack_from('<Q', data, 8)[0] == 0x200000
    assert struct.unpack_from('<Q', data, 16)[0] >= len(data)
    return image


def stage_image(folder, address):
    source = folder/'stage.S'
    source.write_text(f'''
.section .text
.global _start
_start:
    li t0, {address:#x}
    jr t0
''')
    return source


def run_case(args, address, force_sv39, cpu_sv48):
    expected_mode = 8 if force_sv39 or not cpu_sv48 else 9
    folder = args.output_dir/f'{address:x}-force{int(force_sv39)}-sv48{int(cpu_sv48)}'
    source = (REPO/'ostd/src/arch/riscv/boot/bsp_boot.S').read_text()
    source = source.replace('{force_sv39}', str(int(force_sv39)))
    compiler = (['clang', '--target=riscv64-unknown-elf', '-fuse-ld=lld']
                if args.clang else [args.cross_compile+'gcc'])
    objcopy = args.objcopy or args.cross_compile+'objcopy'
    image = compile_image(compiler, objcopy, folder, source, address, expected_mode)
    kernel = image
    loaders = []
    if address != 0x80200000:
        stage = stage_image(folder, address)
        elf = folder/'stage.elf'
        subprocess.run([*compiler, '-nostdlib', '-static', '-fno-pie', '-no-pie',
                        '-march=rv64imac', '-mabi=lp64', '-mcmodel=medany',
                        '-Wl,--build-id=none', '-Wl,-Ttext=0x80200000',
                        str(stage), '-o', str(elf)], check=True, capture_output=True)
        kernel = folder/'stage.bin'
        subprocess.run([objcopy, '-O', 'binary', str(elf), str(kernel)], check=True)
        loaders = ['-device', f'loader,file={image},addr={address:#x},force-raw=on']
    cpu = 'rv64,h=false,sv57=false,sv48='+str(cpu_sv48).lower()
    # Keep the physical address span large without reserving 20 GiB of
    # anonymous host memory. Only bootstrap-touched pages occupy this file.
    backing = folder/'ram.sparse'
    command = [args.qemu, '-M', 'virt,memory-backend=ram',
               '-object', f'memory-backend-file,id=ram,size=20G,mem-path={backing},share=on,prealloc=off',
               '-cpu', cpu, '-smp', '1', '-m', '20G',
               '-nographic', '-monitor', 'none', '-bios', 'default',
               '-kernel', str(kernel), *loaders]
    started = time.monotonic()
    logfile = folder/'boot.log'
    with logfile.open('w') as log:
        proc = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT)
        try:
            while time.monotonic()-started < 15:
                text = logfile.read_text(errors='replace')
                if 'ASTER_BOOT_RELOCATION_' in text or proc.poll() is not None:
                    break
                time.sleep(.05)
        finally:
            if proc.poll() is None:
                proc.terminate()
            proc.wait(timeout=5)
            backing.unlink(missing_ok=True)
    text = logfile.read_text(errors='replace')
    record = dict(passed='ASTER_BOOT_RELOCATION_OK' in text and
                  'ASTER_BOOT_RELOCATION_FAIL' not in text,
                  load_address=hex(address), force_sv39=force_sv39,
                  cpu_sv48=cpu_sv48, expected_satp_mode=expected_mode,
                  command=command, elapsed_seconds=round(time.monotonic()-started, 3))
    (folder/'result.json').write_text(json.dumps(record, indent=2)+'\n')
    print(json.dumps(record), flush=True)
    return record


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir', type=Path, required=True)
    parser.add_argument('--cross-compile', default='riscv64-linux-gnu-')
    parser.add_argument('--clang', action='store_true')
    parser.add_argument('--objcopy')
    parser.add_argument('--qemu', default='qemu-system-riscv64')
    args = parser.parse_args()
    args.output_dir = args.output_dir.resolve()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    results = [run_case(args, address, force, sv48)
               for address in [0x80200000, 0x140200000, 0x400200000]
               for force in [False, True] for sv48 in [False, True]]
    record = dict(passed=all(r['passed'] for r in results), cases=len(results),
                  scope='Actual Asterinas Image header, BSP/MMU and fixed-VMA handoff; assembly Rust-entry substitute. Not a complete Asterinas or SG2044 hardware test.',
                  results=results)
    (args.output_dir/'results.json').write_text(json.dumps(record, indent=2)+'\n')
    raise SystemExit(0 if record['passed'] else 1)


if __name__ == '__main__':
    main()
