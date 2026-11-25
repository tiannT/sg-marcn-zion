#!/bin/bash
# Invoke existing Zion CLI and original Guest programs on SG2044 VM0.
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
base=${ZION_ENTRYPOINT:-/root/zion-tests}
case_id=${1:-}
case "$case_id" in 000[1-9]|0010) ;; *) echo 'Usage: ./run-case.sh 0001..0010' >&2; exit 2;; esac
[[ $EUID == 0 ]] || { echo 'Run as root on SG2044 VM0' >&2; exit 2; }
out=$(mktemp -d "$base/logs/aligned-2-3-$case_id-$(date -u +%Y%m%dT%H%M%SZ)-XXXXXX")
exec > >(tee "$out/stdout.log") 2> >(tee "$out/stderr.log" >&2)
finish() {
 rc=$?
 trap - EXIT
 printf 'case=2-3-%s exit=%s log=%s\n' "$case_id" "$rc" "$out"
 python3 - "$out" "$case_id" "$rc" <<'RESULT_PY'
import datetime,json,sys
from pathlib import Path
p=Path(sys.argv[1]);p.joinpath('result.json').write_text(json.dumps(dict(case='2-3-'+sys.argv[2],exit_code=int(sys.argv[3]),finished_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),host_boot_id=Path('/proc/sys/kernel/random/boot_id').read_text().strip()),indent=2)+'\n')
RESULT_PY
 exit "$rc"
}
trap finish EXIT
printf 'CASE=2-3-%s UTC=%s\n' "$case_id" "$(date -u +%FT%TZ)"
printf 'BOOT_ID='; cat /proc/sys/kernel/random/boot_id
cd "$base"
guest() { ssh -o IdentityAgent=none -o ForwardAgent=no -o BatchMode=yes -o ConnectTimeout=5 -p 10022 root@127.0.0.1 "$@"; }
start_asterinas() {
 ./zion-runtime.sh init
 ./zion-runtime.sh select asterinas
 sh ./zion-runtime.sh guest
 guest 'test -x /usr/sbin/mysqld && test -x /usr/local/redis/bin/redis-server && test -x /benchmark/bin/sqlite-speedtest1'
}
case "$case_id" in
 0001)
  start_asterinas
  ./zion-runtime.sh status
  guest 'uname -a; /usr/bin/busybox --help 2>&1 | head -1; cat /proc/version; grep MemTotal /proc/meminfo'
  ;;
 0002)
  start_asterinas
  guest '/benchmark/bin/sqlite-speedtest1 --size 10; rc=$?; printf "sqlite_exit=%s\n" "$rc"; exit "$rc"'
  ;;
 0003|0004)
  start_asterinas
  label=redis; [[ $case_id != 0004 ]] || label=mysql
  guest 'sh -s' < "$root/guest/test-$label.sh"
  ;;
 0005)
  test "$(cloc --version)" = 2.02
  cd "${ZION_STATS_SOURCE:?Set ZION_STATS_SOURCE to the fixed Asterinas statistics checkout}"
  printf 'SOURCE_COMMIT=536593f341c581d430f6743cfd322c09e4e0be26\n'
  python3 tools/count_unsafe_rust.py
  ;;
 0006)
  grep -i '^isa' /proc/cpuinfo
  printf 'NOT_APPLICABLE: SG2044 has no physical H extension.\n'
  exit 77
  ;;
 0007)
  ./zion-runtime.sh init
  ./zion-runtime.sh protect
  ;;
 0008)
  printf 'Physical serial must already be recording this Host boot.\n'
  ./zion-runtime.sh init
  test "$(cat /sys/module/kvm/parameters/zion_vcpu_tamper)" = 0
  printf '1\n' > /sys/module/kvm/parameters/zion_vcpu_tamper
  ./zion-runtime.sh linux
  test "$(cat /sys/module/kvm/parameters/zion_vcpu_tamper)" = 0
  guest 'set -e; uname -a; ifconfig eth0; dd if=/dev/vda of=/tmp/blk-read bs=512 count=1; wc -c /tmp/blk-read'
  ;;
 0009)
  ./zion-runtime.sh init
  ./zion-runtime.sh linux
  ./zion-runtime.sh status
  guest 'set -e; uname -a; /usr/bin/busybox --help 2>&1 | head -1; ifconfig eth0; dd if=/dev/vda of=/tmp/blk-read bs=512 count=1; wc -c /tmp/blk-read'
  ;;
 0010)
  ./zion-runtime.sh init
  ./zion-runtime.sh enclave
  guest 'test ! -e /dev/vda && test ! -e /sys/class/block/vda'
  guest '/usr/bin/run-zion-enclave-demo; rc=$?; printf "demo_exit=%s\n" "$rc"; exit "$rc"'
  ;;
esac
