#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""Prepare SG2044 PMP recovery card files; never flash, mount, or reboot."""
import argparse
import json
from pathlib import Path
import shutil

from build_guest_bundle import build_bundle
from test_pmp_recovery import build_payload, sha


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--firmware-dir', type=Path, required=True)
    ap.add_argument('--boot-template', type=Path, required=True)
    ap.add_argument('--board-dtb', type=Path, required=True)
    ap.add_argument('--output-dir', type=Path, required=True)
    ap.add_argument('--cross-compile', default='riscv64-linux-gnu-')
    args = ap.parse_args()
    build = json.loads((args.firmware_dir/'build-manifest.json').read_text())
    firmware = args.firmware_dir/'fw_dynamic.bin'
    if build['platform'] != 'sg2044' or build['static_guests'] != 'linux':
        ap.error('Use the production-layout SG2044 Linux-profile candidate, not a QEMU/smoke binary')
    if sha(firmware) != build['artifacts']['fw_dynamic.bin']['sha256']:
        ap.error('Firmware hash disagrees with its build manifest')
    names = {'fsbl.bin','zsbl.bin','fw_dynamic.bin','SG2044.fd','sg2044-evb.dtbo','sg2044-sra3.dtbo'}
    if not all((args.boot_template/name).is_file() for name in names):
        ap.error('Boot template must contain the six matching boot components')
    args.output_dir.mkdir(parents=True, exist_ok=True)
    cases = {}
    for vm in range(2):
        for access in ('read','write','execute'):
            label = f'VM{vm}_{access}'
            out = args.output_dir/label
            card = out/'riscv64'
            card.mkdir(parents=True, exist_ok=True)
            images, commands, address = build_payload(
                out, vm, access, 'peer', args.cross_compile, board=True)
            for name in names - {'fw_dynamic.bin','SG2044.fd'}:
                shutil.copy2(args.boot_template/name, card/name)
            shutil.copy2(firmware, card/'fw_dynamic.bin')
            payload = build_bundle(images,[None,None],[f'scheme-b-pmp-recover {label}']*2,
                                   card/'SG2044.fd',args.board_dtb)
            shutil.move(str(card/'SG2044.fd.json'),str(out/'payload-manifest.json'))
            markers = [f'[RECOVER VM{vm}] ACCESS_DENIED_RECOVERED errno=-14']
            for guest in range(2):
                markers += [f'[SCHEME_B] VM{guest} PMP_DENIAL_POLICY=GUEST_TRAP_RECOVERY',
                            f'[RECOVER VM{guest}] PRIVATE_CANARY_OK',
                            f'[RECOVER VM{guest}] SHARED_AFTER_DENIAL_OK',
                            f'[RECOVER VM{guest}] ALIVE_AFTER_DENIAL TIMER_TICKS=3']
            record = dict(probe_vm=vm,access=access,address=hex(address),
                          hardware_test_performed=False,expected_markers=markers,
                          expected_recovery_count=3,payload=payload,compile_commands=commands,
                          files={name:dict(bytes=(card/name).stat().st_size,sha256=sha(card/name))
                                 for name in sorted(names)})
            (out/'test-manifest.json').write_text(json.dumps(record,indent=2)+'\n')
            cases[label] = record
    manifest = dict(scope='Prepared SG2044 native recovery diagnostics. Two bare S-mode programs; '
                          'Linux and original SSH do not run during this independent test boot. '
                          'No NVMe driver, mount, disk image writer or reboot is invoked.',
                    hardware_test_performed=False,firmware_sha256=sha(firmware),cases=cases)
    (args.output_dir/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print(json.dumps(dict(prepared_cases=len(cases),hardware_test_performed=False,
                          firmware_sha256=sha(firmware)),indent=2))


if __name__ == '__main__':
    main()
