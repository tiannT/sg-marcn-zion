#!/bin/sh
# Stable Zion command name; callable with either ./ or sh.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)
exec python3 "$root/compat/zion_runtime.py" "$@"
