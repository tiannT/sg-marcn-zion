#!/usr/bin/env bash
# Explicit integration verification. Switches VM1 and clears its RAM.
# Does NOT execute a real S3 injection; preserves the first-alert window.
set -euo pipefail
unset SSH_AUTH_SOCK SSH_AGENT_PID SSHPASS
builtin cd -- "${ZION_COMPAT_ENTRYPOINT:-/root/zion-tests}"
install_root=$(pwd -P)
logs="$(mktemp -d "$install_root/logs/compat-verification.XXXXXXXX")"
printf 'COMPAT_VERIFICATION_LOGS=%s\n' "$logs"
exec > >(tee "$logs/full.log") 2>&1
./zion-runtime.sh init
./zion-runtime.sh status --json > "$logs/before.json"
./zion-runtime.sh init
./zion-runtime.sh status --json > "$logs/after-second-init.json"
python3 - "$logs/before.json" "$logs/after-second-init.json" <<'PY'
import json,sys
a,b=[json.load(open(p)) for p in sys.argv[1:]]
assert a['host_boot_id']==b['host_boot_id']
assert a['runtime']==b['runtime'] and a['warnings']==b['warnings']
print('INIT_IDEMPOTENT_PASS')
PY
./zion-runtime.sh tamper-s3 1
./zion-runtime.sh tamper-s3 0
./zion-runtime.sh tamper-s3 status
./zion-runtime.sh protect | tee "$logs/pmp-linux.log"
grep -q SIX_CASES_PASSED "$logs/pmp-linux.log"
./zion-runtime.sh select asterinas
sh ./zion-runtime.sh guest
ssh -p 10022 root@127.0.0.1 'set -e; id; uname -a; cat /proc/version; printf "ASTERINAS_SSH_OK\n"' </dev/null
./zion-runtime.sh status > "$logs/asterinas-status.log"
ssh -p 10022 root@127.0.0.1 'set -e; /benchmark/bin/sqlite-speedtest1 --size 10; printf "sqlite_exit=0\n"' \
    </dev/null | tee "$logs/sqlite.log"
grep -q TOTAL "$logs/sqlite.log"
ssh -p 10022 root@127.0.0.1 'sh -s' <<'GUEST' | tee "$logs/security-apps.log"
set -eu
token=$(/usr/bin/tee-crypto enc mypassword 'hello CVM')
test "${#token}" -eq 608
test "$(/usr/bin/tee-crypto dec mypassword "$token")" = 'hello CVM'
if /usr/bin/tee-crypto dec wrongpassword "$token" > /tmp/compat-wrong; then exit 1; fi
test ! -s /tmp/compat-wrong
case "$token" in *0) changed="${token%?}1";; *) changed="${token%?}0";; esac
if /usr/bin/tee-crypto dec mypassword "$changed" > /tmp/compat-changed; then exit 1; fi
test ! -s /tmp/compat-changed
echo TEE_CRYPTO_ROUNDTRIP_AND_REJECTION_PASS
/usr/bin/tee-vault put acceptance-key '0123456789abcdef-PlanB-test-key'
test "$(/usr/bin/tee-vault get acceptance-key)" = '0123456789abcdef-PlanB-test-key'
test "$(stat -c %a /run/tee-vault)" = 700
test "$(stat -c %a /run/tee-vault/.master)" = 600
test "$(stat -c %a /run/tee-vault/acceptance-key.gcm)" = 600
if /usr/bin/tee-vault get ../master; then exit 1; fi
echo TEE_VAULT_PUT_GET_AND_PERMISSIONS_PASS
GUEST
./zion-runtime.sh protect | tee "$logs/pmp-restore-asterinas.log"
./zion-runtime.sh status --json > "$logs/restored-asterinas.json"
python3 - "$logs/restored-asterinas.json" <<'PY'
import json,sys
assert json.load(open(sys.argv[1]))['runtime']['image']==1
print('PROTECT_RESTORES_PRIOR_ASTERINAS_PASS')
PY
./zion-runtime.sh stop
./zion-runtime.sh status --json > "$logs/stopped.json"
python3 - "$logs/stopped.json" <<'PY'
import json,sys
assert json.load(open(sys.argv[1]))['runtime']['state']==0
print('CONTROLLED_STOP_PASS')
PY
./zion-runtime.sh linux
./zion-runtime.sh status --json > "$logs/before-enclave-alias.json"
./zion-runtime.sh enclave
./zion-runtime.sh status --json > "$logs/after-enclave-alias.json"
python3 - "$logs/before-enclave-alias.json" "$logs/after-enclave-alias.json" <<'PY'
import json,sys
a,b=[json.load(open(p)) for p in sys.argv[1:]]
assert a['runtime']['epoch']==b['runtime']['epoch'] and b['runtime']['image']==2
print('ENCLAVE_ALIAS_SAME_LINUX_BOOT_PASS')
PY
ssh -p 10022 root@127.0.0.1 'set -e; export PATH=/usr/sbin:/usr/bin:/sbin:/bin; uname -a; ifconfig eth0; cat /sys/class/net/eth0/device/modalias; cat /sys/block/vda/device/modalias; dd if=/dev/vda of=/tmp/blk-read bs=512 count=1; wc -c /tmp/blk-read' </dev/null
ssh -p 10022 root@127.0.0.1 'head -c 512 /dev/vda' </dev/null > "$logs/vda.bin"
head -c 512 /run/planb-runtime/linux.img > "$logs/backing.bin"
cmp "$logs/vda.bin" "$logs/backing.bin"
test "$(wc -c < "$logs/vda.bin")" -eq 512
echo VIRTIO_BLOCK512_MATCH
ssh -p 10022 root@127.0.0.1 '/usr/bin/run-zion-enclave-demo; rc=$?; printf "demo_exit=%s\n" "$rc"; exit "$rc"' \
    </dev/null > "$logs/enclave-demo.log" 2>&1 &
demo_pid=$!
for ((attempt=0; attempt<80; attempt++)); do
    grep -q 'COMPUTE  1/10' "$logs/enclave-demo.log" && break
    kill -0 "$demo_pid" 2>/dev/null || { cat "$logs/enclave-demo.log"; exit 1; }
    sleep .1
done
grep -q 'COMPUTE  1/10' "$logs/enclave-demo.log"
if ./zion-runtime.sh stop > "$logs/live-enclave-stop.log" 2>&1; then
    echo 'FAIL: live Enclave stop was accepted'; exit 1
fi
grep -qi 'busy' "$logs/live-enclave-stop.log"
echo LIVE_ENCLAVE_STOP_REJECTED_PASS
wait "$demo_pid"
cat "$logs/enclave-demo.log"
test "$(grep -c VERIFIED "$logs/enclave-demo.log")" -eq 10
grep -q 'PASS: all negative ABI tests' "$logs/enclave-demo.log"
grep -q 'demo_exit=0' "$logs/enclave-demo.log"
ssh -p 10022 root@127.0.0.1 'sh -s' <<'GUEST' | tee "$logs/enclave-guard.log"
set -e
export PATH=/usr/sbin:/usr/bin:/sbin:/bin
/root/zion-enclave/planb-enclave-guard /root/zion-enclave/planb-context-probe.bin
insmod /root/zion-enclave/planb-context-audit.ko
trap 'rmmod planb_context_audit 2>/dev/null || true' EXIT
test "$(cat /sys/module/planb_context_audit/parameters/last_passed)" = Y
/usr/bin/busybox dmesg | /usr/bin/busybox grep 'PLANB ENCLAVE AUDIT'
rmmod planb_context_audit
trap - EXIT
GUEST
./zion-runtime.sh stop
./zion-runtime.sh linux
./zion-runtime.sh status --json > "$logs/final.json"
python3 - "$logs/before.json" "$logs/final.json" <<'PY'
import json,sys
a,b=[json.load(open(p)) for p in sys.argv[1:]]
assert a['host_boot_id']==b['host_boot_id']
assert a['warnings']==b['warnings']==0
assert b['runtime']['state']==1 and b['runtime']['image']==2
assert b['runtime']['clean']==15 and b['runtime']['error']==0
assert b['tamper']['phase']=='cancelled'
print('COMPAT_INTEGRATION_PASS; S3 first-alert window preserved; Host not rebooted')
PY
