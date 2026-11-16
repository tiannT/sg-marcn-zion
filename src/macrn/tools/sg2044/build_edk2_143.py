#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""Build the reviewed 1.4.3 UEFI candidate. Does not package or flash firmware."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess

HERE = Path(__file__).resolve().parent


def run_text(command, cwd=None):
    return subprocess.check_output(command, cwd=cwd, text=True).strip()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--sdk", required=True, type=Path)
    p.add_argument("--cross-compile", default="riscv64-linux-gnu-")
    p.add_argument("--iasl-dir", type=Path)
    p.add_argument("--python", default="/usr/bin/python3")
    p.add_argument("--jobs", type=int, default=4)
    p.add_argument("--board", choices=["SD3-10", "SRA3-40"], default="SD3-10")
    p.add_argument("--module-only", action="store_true")
    p.add_argument("--output-dir", type=Path)
    args = p.parse_args()
    sdk = args.sdk.resolve()
    pins = json.loads((HERE / "vendor-patches-1.4.3/source-pins.json").read_text())
    if args.board == "SRA3-40":
        pins["reference_default_board_profile"] = pins["board_profile"]
        pins["board_profile"] = args.board
        pins["declared_board_name"] = "SRA3"
        pins["initial_target_dmi_board_name"] = pins.pop("target_board_name")
        pins["board_selection_basis"] = "user-supplied vendor instructions, 2026-11-10"
    expected = {".": pins["wrapper"]["commit"], **pins["modules"]}
    for path, commit in expected.items():
        actual = run_text(["git", "rev-parse", "HEAD"], sdk / path)
        if actual != commit:
            p.error(f"Wrong source revision in {path}: {actual}, expected {commit}")
    platform = sdk / "edk2-platforms"
    patch = HERE / "vendor-patches-1.4.3/0001-edk2-reserve-macrn.patch"
    check = subprocess.run(["git", "apply", "--reverse", "--check", str(patch)],
                           cwd=platform, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if check.returncode:
        p.error("Apply the 1.4.3 memory reservation patch before building")
    board_patch = HERE / "vendor-patches-1.4.3/0004-sra3-40-reserve-macrn.patch"
    if args.board == "SRA3-40":
        check = subprocess.run(
            ["git", "apply", "--reverse", "--check", str(board_patch)],
            cwd=platform, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        if check.returncode:
            p.error("Apply the SRA3-40 memory reservation patch before building")
    link_patch = HERE / "vendor-patches-1.4.3/0002-edk2-linux-cross-no-pie.patch"
    check = subprocess.run(["git", "apply", "--reverse", "--check", str(link_patch)],
        cwd=sdk / "edk2", stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if check.returncode:
        p.error("Apply the RISC-V non-PIE toolchain patch before building")
    # edksetup retains an existing generated configuration. Keep its RISC-V
    # link setting consistent without replacing any other local settings.
    conf = sdk / "edk2/Conf/tools_def.txt"
    if conf.exists():
        old = b"DEFINE GCC5_RISCV_ALL_DLINK_COMMON                = -nostdlib -Wl,"
        new = b"DEFINE GCC5_RISCV_ALL_DLINK_COMMON                = -nostdlib -no-pie -Wl,"
        data = conf.read_bytes()
        if old in data:
            conf.write_bytes(data.replace(old, new))
    env = os.environ.copy()
    env.update(WORKSPACE=str(sdk),
               PACKAGES_PATH=":".join(str(sdk / x) for x in
                   ["edk2", "edk2-platforms", "edk2-non-osi", "external-modules"]),
               EDK_TOOLS_PATH=str(sdk / "edk2/BaseTools"),
               GCC5_RISCV64_PREFIX=args.cross_compile, PYTHON_COMMAND=args.python)
    env.setdefault("SOURCE_DATE_EPOCH", "1769990400")
    if args.iasl_dir:
        env["IASL_PREFIX"] = str(args.iasl_dir.resolve()) + "/"
    out = (args.output_dir or sdk / "macrn-build-evidence").resolve()
    out.mkdir(parents=True, exist_ok=True)
    mode = "memory-module" if args.module_only else "firmware"
    report = out / (mode + "-report.txt")
    command = ["build", "-n", str(args.jobs), "-a", "RISCV64", "-t", "GCC5",
               "-b", "RELEASE", "-p",
               f"Platform/Sophgo/SG2044Pkg/{args.board}/{args.board}.dsc",
               "-y", str(report), "-Y", "PCD"]
    if args.module_only:
        command += ["-m", "Silicon/Sophgo/Modules/MemoryInitPei/MemoryInitPei.inf"]
    script = "set -e\nsource edk2/edksetup.sh\n" + shlex.join(command) + "\n"
    log = out / (mode + "-build.log")
    with log.open("w") as f:
        result = subprocess.run(["bash", "--noprofile", "--norc"], input=script,
            text=True, env=env, cwd=sdk, stdout=f, stderr=subprocess.STDOUT)
    build = sdk / f"Build/{args.board}/RELEASE_GCC5"
    files = [build / "RISCV64/MemoryInitPei.efi"]
    if not args.module_only:
        files += [build / f"FV/{args.board}.fd"]
    metadata = {"exit": result.returncode, "sdk": str(sdk), "sources": pins,
                "selected_board_profile": args.board,
                "board_selection_basis": ("user-supplied vendor instructions"
                    if args.board == "SRA3-40" else "initial DMI-based candidate"),
                "command": command, "log": str(log),
                "patch_sha256": hashlib.sha256(patch.read_bytes()).hexdigest(),
                "toolchain_patch_sha256": hashlib.sha256(link_patch.read_bytes()).hexdigest(),
                "compiler": run_text([args.cross_compile + "gcc", "--version"]).splitlines()[0],
                "artifacts": {str(f.relative_to(build)): {
                    "bytes": f.stat().st_size,
                    "sha256": hashlib.sha256(f.read_bytes()).hexdigest()}
                    for f in files if f.is_file()},
                "hardware_boot_verified": False}
    if args.board == "SRA3-40":
        metadata["board_patch_sha256"] = hashlib.sha256(board_patch.read_bytes()).hexdigest()
    (out / (mode + "-manifest.json")).write_text(json.dumps(metadata, indent=2) + "\n")
    print(json.dumps(metadata, indent=2))
    if result.returncode:
        print(log.read_text()[-5000:])
    raise SystemExit(result.returncode)


if __name__ == "__main__":
    main()
