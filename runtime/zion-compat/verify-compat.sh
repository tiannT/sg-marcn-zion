#!/usr/bin/env bash
# Explicit integration checks; switches VM1 and clears Guest runtime data.
# Physical-serial tests are performed separately using the test manual.
set -euo pipefail
unset SSH_AUTH_SOCK SSH_AGENT_PID SSHPASS
script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
export ZION_ENTRYPOINT=${ZION_COMPAT_ENTRYPOINT:-/root/zion-tests}
for case_id in 0001 0002 0003 0004 0009 0010; do
    bash "$script_dir/../run-case.sh" "$case_id"
done
