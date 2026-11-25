#!/usr/bin/env python3
"""Package an explicit set of local inputs; never deploy or flash hardware."""
import argparse
import importlib.util
import json
from pathlib import Path
import shutil
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for role in ['vm0', 'asterinas', 'linux']:
        parser.add_argument('--'+role+'-kernel', type=Path, required=True)
        parser.add_argument('--'+role+'-initrd', type=Path, required=True)
        parser.add_argument('--'+role+'-cmdline-file', type=Path, required=True)
    parser.add_argument('--board-dtb', type=Path, required=True)
    parser.add_argument('--firmware', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, required=True)
    args = parser.parse_args()
    output = args.output_dir.resolve()
    if output.exists():
        parser.error('Use a new output directory so a previous boot seed is never reused')
    roles = ['vm0', 'asterinas', 'linux']
    kernels = [getattr(args, role+'_kernel').resolve() for role in roles]
    initrds = [getattr(args, role+'_initrd').resolve() for role in roles]
    commands = []
    for role in roles:
        command = getattr(args, role+'_cmdline_file').read_text().rstrip('\n')
        if '\n' in command or '\x00' in command:
            parser.error('Each command-line file must contain one line without NUL bytes')
        commands.append(command)
    for path in [*kernels, *initrds, args.board_dtb, args.firmware]:
        if not path.is_file():
            parser.error('Required input does not exist: '+str(path))
    entropy = load('planb_entropy', ROOT/'scripts/provision_test_entropy.py')
    bundle = load('planb_bundle', ROOT/'src/macrn/tools/sg2044/build_runtime_bundle.py')
    native = load('native_fdt', ROOT/'scripts/native_fdt.py')
    seeded_dtb = entropy.provision(args.board_dtb.read_bytes())
    output.mkdir(parents=True, mode=0o700)
    with tempfile.TemporaryDirectory(prefix='planb-private-') as temporary:
        dtb = Path(temporary)/'board.dtb'
        dtb.write_bytes(seeded_dtb)
        dtb.chmod(0o600)
        native.sanitize(dtb)
        result = bundle.build_bundle(kernels, initrds, commands, output/'SG2044.fd', dtb)
    shutil.copyfile(args.firmware, output/'fw_dynamic.bin')
    (output/'fw_dynamic.bin').chmod(0o600)
    print(json.dumps(result, ensure_ascii=False, indent=2))


if __name__ == '__main__':
    main()
