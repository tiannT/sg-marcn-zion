#!/usr/bin/env bash
set -euo pipefail
project_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
git -C "$project_root" submodule sync -- src/linux-cvm-kernel
git -C "$project_root" submodule update --init --depth=1 -- src/linux-cvm-kernel
expected=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["linux-cvm-kernel"]["upstream_commit"])' "$project_root/UPSTREAM_VERSIONS.json")
actual=$(git -C "$project_root/src/linux-cvm-kernel" rev-parse HEAD)
[[ $actual == "$expected" ]] || { printf 'Linux submodule commit does not match the pinned version\n' >&2; exit 1; }
printf 'LINUX_SUBMODULE_READY %s\n' "$actual"
