#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# VM0 controller for the unified Plan B image. Kernel selection stays in M-mode.
set -euo pipefail
umask 077
base=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
ctl=$base/planb-guestctl
backend=$base/planb-backend
run=/run/planb-runtime
[[ $EUID == 0 ]] || { echo 'Run as root in VM0.' >&2; exit 2; }
# Reject old firmware before creating interfaces or opening shared memory.
"$ctl" --status >/dev/null
[[ ! -L $run ]] || { echo 'Invalid runtime state directory.' >&2; exit 2; }
mkdir -p -- "$run"
[[ $(stat -c %u "$run") == 0 ]] || exit 2
chmod 700 "$run"
exec 9>"$run/admin.lock"
flock -x 9
boot_id=$(cat /proc/sys/kernel/random/boot_id)
state= image= epoch=
read_status() {
    local snapshot
    snapshot=$("$ctl" --status)
    [[ $snapshot =~ \"state\":([0-9]+) ]] || return 1; state=${BASH_REMATCH[1]}
    [[ $snapshot =~ \"image\":([0-9]+) ]] || return 1; image=${BASH_REMATCH[1]}
    [[ $snapshot =~ \"epoch\":([0-9]+) ]] || return 1; epoch=${BASH_REMATCH[1]}
    printf '%s\n' "$snapshot"
}
backend_live() {
    local recorded_boot saved_start actual_start
    [[ -f $run/backend.pid && ! -L $run/backend.pid ]] || return 1
    read -r backend_pid saved_start recorded_boot < "$run/backend.pid"
    [[ $backend_pid =~ ^[0-9]+$ && $saved_start =~ ^[0-9]+$ && $recorded_boot == "$boot_id" ]] || return 2
    [[ -d /proc/$backend_pid ]] || return 1
    read -ra backend_stat < "/proc/$backend_pid/stat"
    actual_start=${backend_stat[21]}
    [[ $actual_start == "$saved_start" && $(cat "/proc/$backend_pid/comm") == planb-backend ]] || return 2
}
stop_backend() {
    local rc=0
    backend_live || rc=$?
    if [[ $rc == 2 ]]; then echo 'Backend PID identity mismatch; refusing to signal it.' >&2; return 1; fi
    if [[ $rc == 0 ]]; then
        kill -TERM "$backend_pid"
        for ((i=0; i<100; ++i)); do
            [[ -d /proc/$backend_pid ]] || break
            # A zombie already closed its TAP and disk descriptors.
            read -ra backend_stat < "/proc/$backend_pid/stat" || break
            [[ ${backend_stat[2]} == Z ]] && break
            sleep .05
        done
        [[ ! -d /proc/$backend_pid || ${backend_stat[2]:-} == Z ]] || {
            echo 'Backend did not exit; Guest remains stopped.' >&2; return 1;
        }
    fi
    rm -f -- "$run/backend.pid" "$run/backend-epoch"
}
stop_guest() {
    "$ctl" --stop  # EBUSY leaves a live enclave/Guest/backend unchanged.
    stop_backend
}
attach_backend() {
    local profile=$1 tap_addr create= rc=0
    local tamper=()
    [[ ${PLANB_TEST_TAMPER_S3:-0} == 1 ]] && tamper=(--tamper-s3-once)
    backend_live || rc=$?
    if [[ $rc == 2 ]]; then echo 'Backend PID identity mismatch.' >&2; return 1; fi
    if [[ $rc == 0 ]]; then
        [[ $(cat "$run/backend-epoch") == "$epoch" ]] || { echo 'Backend belongs to another Guest epoch.' >&2; return 1; }
        return 0
    fi
    case $profile in
        asterinas) tap_addr=10.0.2.2 ;;
        linux) tap_addr=10.0.3.1 ;;
        *) return 2 ;;
    esac
    [[ -d /sys/module/tun ]] || modprobe tun
    [[ -e $run/$profile.img ]] || create=--create-disk
    # /run is RAM-backed; the backend requires a regular 512 MiB file and
    # creates only a new TAP. It rejects existing routes/interfaces/conflicts.
    nohup "$backend" --disk "$run/$profile.img" ${create:+"$create"} \
        --tap pbrt0 --tap-address "$tap_addr" "${tamper[@]}" \
        </dev/null >"$run/backend-$epoch.log" 2>&1 9>&- &
    backend_pid=$!
    for ((i=0; i<100; ++i)); do
        if grep -q PLANB_BACKEND_READY "$run/backend-$epoch.log"; then break; fi
        kill -0 "$backend_pid" 2>/dev/null || { cat "$run/backend-$epoch.log" >&2; return 1; }
        sleep .1
    done
    grep -q PLANB_BACKEND_READY "$run/backend-$epoch.log" || { kill -TERM "$backend_pid"; return 1; }
    read -ra backend_stat < "/proc/$backend_pid/stat"
    printf '%s %s %s\n' "$backend_pid" "${backend_stat[21]}" "$boot_id" > "$run/backend.pid"
    printf '%s\n' "$epoch" > "$run/backend-epoch"
}
usage() { echo "Usage: $0 status | stop | start asterinas|linux | switch asterinas|linux" >&2; exit 2; }
command=${1:-status}
case $command in
    status)
        [[ $# -le 1 ]] || usage
        read_status
        "$backend" --status
        ;;
    stop)
        [[ $# == 1 ]] || usage
        stop_guest
        ;;
    start|switch)
        [[ $# == 2 ]] || usage
        profile=$2
        case $profile in asterinas) wanted=1;; linux) wanted=2;; *) usage;; esac
        read_status
        if [[ $command == switch && $state == 1 ]]; then
            stop_guest
            state=0
        fi
        if [[ $state == 0 ]]; then
            stop_backend
            "$ctl" --start "$profile"
            read_status
        fi
        [[ $state == 1 && $image == "$wanted" ]] || {
            echo 'Guest is running another image; use switch.' >&2; exit 1;
        }
        if ! attach_backend "$profile"; then
            "$ctl" --stop || true
            stop_backend || true
            exit 1
        fi
        printf 'GUEST_READY_FOR_BOOT image=%s epoch=%s backend_log=%s\n' "$profile" "$epoch" "$run/backend-$epoch.log"
        ;;
    *) usage ;;
esac
