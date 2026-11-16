# SG2044 方案 B 测试手册：Zion 原命令兼容版

本手册列出测试输入、预期输出和判据。先按[首次部署指南](getting-started.md)构建并安装匹配的固件、运行工具、Guest 用户态和本地登录身份，再执行本手册。架构与能力边界见[设计说明](design.md)。

输出示例用于说明应检查的行为；用户需在自己的目标板上执行并保存结果。

## 1. 执行位置与安装布局

所有测试命令在 **目标板管理 Linux（VM0）的 root Bash** 中执行。上层使用 `zion-runtime.sh`、标准 SSH 和 Guest 应用命令；兼容层转接方案 B 控制工具。Guest 登录使用安装时配置的 Host 本地身份，无需 SSH agent 转发。

| 路径 | 内容 |
|---|---|
| `/root/zion-tests/` | 默认命令入口，链接到安装前缀 |
| `/opt/sg-marcn-zion/` | 默认安装前缀；`zion-runtime.sh` 为入口，`compat/` 为适配源码 |
| `<prefix>/state/` | Guest 选择、Host 启动 ID、注入状态和 SSH 代理记录 |
| `<prefix>/logs/` | 兼容层与本轮测试日志 |
| `/opt/sg-marcn-zion-tools/` | 默认运行工具目录 |
| `/opt/sg-marcn-zion-source/asterinas/` | 0003 的默认源码统计输入 |

自定义安装时替换为实际目录。兼容层源码位于 `runtime/zion-compat/`，运行工具源码位于 `src/macrn/`；部署依赖见[构建说明](build.md)。

## 2. 原命令与执行规则

| 上层命令 | 方案B中的实际动作 |
|---|---|
| `./zion-runtime.sh init` | 检查真实ABI、原工具哈希及本机凭据，初始化本轮状态并启动本机SSH转发；重复调用不重载Guest、不重置M-mode告警 |
| `./zion-runtime.sh select asterinas` | 保存Guest选择；也支持`select linux`及交互式选择 |
| `sh ./zion-runtime.sh guest` | 按选择启动Guest并等待SSH就绪 |
| `./zion-runtime.sh asterinas` | 使用Asterinas；已在同一模式运行时保留当前epoch |
| `./zion-runtime.sh linux` | 使用Linux CVM；已在同一模式且没有待注入操作时保留当前epoch |
| `./zion-runtime.sh enclave` | 使用同一份已集成Zion/Enclave组件的Linux CVM，无需另换镜像 |
| `./zion-runtime.sh protect` | 执行Linux场景的真实双向PMP六项；若原来是Asterinas，会临时切Linux，测完恢复Asterinas |
| `./zion-runtime.sh status` | 输出真实状态、后端日志及Guest状态查询；`status --json`提供机器可读状态 |
| `./zion-runtime.sh stop` | 由监控器受控停止Guest并处理后端；有存活Enclave时拒绝停止 |
| `./zion-runtime.sh tamper-s3 1` | 为下一次`linux`启动预置一次S3注入；不立即修改共享字段 |
| `./zion-runtime.sh tamper-s3 0` | 取消尚未提交给后端的预置；不能清除M-mode告警计数 |
| `./zion-runtime.sh tamper-s3 status` | 查询预置/提交/消费状态及实际告警计数 |

Guest使用统一登录命令：

```bash
ssh -p 10022 root@127.0.0.1
```

10022 只监听目标 Host 本机回环地址，按运行的 VM1 转发到 Asterinas 或 Linux。客户端使用安装时配置的 Guest 身份；授权公钥须预先放入 Guest rootfs。

Guest模式变化会擦除并重载VM1私有RAM，Guest临时文件与vault数据不保留。同模式普通启动命令保持幂等；0008有待注入操作时，`linux`会明确执行一次受控重载。

## 3. 测试前准备

在目标 Host root Bash 中执行，自定义安装时调整两个路径：

```bash
export ZION_PREFIX=/opt/sg-marcn-zion
cd "$ZION_PREFIX"
set -o pipefail
./zion-runtime.sh init
export PLANB_LOGROOT="$(mktemp -d "$ZION_PREFIX/logs/manual.XXXXXXXX")"
export PLANB_BASE=/opt/sg-marcn-zion-source
cat /proc/sys/kernel/random/boot_id | tee "$PLANB_LOGROOT/host-boot-id.txt"
./zion-runtime.sh status | tee "$PLANB_LOGROOT/initial-status.log"
printf '本轮日志：%s\n' "$PLANB_LOGROOT"
```

预期看到 `[ZION INIT] READY backend=macrn-planb`，`status`反映真实Guest类型、epoch及告警计数。`image=1`为Asterinas，`image=2`为Linux CVM。

完整一轮建议顺序：**0008 → 0005 → 0009 → 0010 → 0001 → 0002 → 0006 → 0007**；0003 在 Host 本机单独统计。按组执行可减少 Guest 切换。章节按大纲编号排列，每节说明所需 Guest。

0008须保存物理串口原始消息；当前Host若`warnings=0`可使用尚未消费的首次窗口，若为1则不能重新测量0→1的首次告警。其他项目不要求为此重启Host。Host重启、固件更换由操作人员安排。

`test`、`grep -q`和`cmp`成功时通常没有输出。需在命令执行后立即用`echo $?`确认返回0；下面的PASS标记均在真实检查成功后输出。

## 4. 九项测试输入、输出和判据

### 4.1 2-3-0001：Asterinas启动与登录

输入：

```bash
./zion-runtime.sh select asterinas
sh ./zion-runtime.sh guest
./zion-runtime.sh status | tee "$PLANB_LOGROOT/0001-host-status.log"
ssh -p 10022 root@127.0.0.1 \
  'set -e; id; uname -a; cat /proc/version; printf "ASTERINAS_SSH_OK\n"' \
  2>&1 | tee "$PLANB_LOGROOT/0001-asterinas-login.log"
```

关键输出：`GUEST_SSH_READY`、`uid=0(root)`、包含 Rust 编译器信息且与构建版本一致的 `/proc/version`，以及 `ASTERINAS_SSH_OK`。

通过判据：运行状态为Asterinas，SSH成功，身份和Rust内核版本正确。Asterinas的`uname`返回Linux兼容标识5.13.0是正常现象；最后的SSH_OK是调用方标记，不能替代内核版本核对。原大纲的启动图形应查看实际串口启动记录，兼容层不伪造图形或串口输出。

### 4.2 2-3-0002：SQLite

输入：

```bash
./zion-runtime.sh asterinas
ssh -p 10022 root@127.0.0.1 'sh -s' <<'GUEST' 2>&1 | tee "$PLANB_LOGROOT/0002-sqlite.log"
if /benchmark/bin/sqlite-speedtest1 --size 10 > /tmp/0002-sqlite.log 2>&1; then
    rc=0
else
    rc=$?
fi
cat /tmp/0002-sqlite.log
printf 'sqlite_exit=%s\n' "$rc"
exit "$rc"
GUEST
grep -q TOTAL "$PLANB_LOGROOT/0002-sqlite.log" \
  && grep -q 'sqlite_exit=0' "$PLANB_LOGROOT/0002-sqlite.log" \
  && printf 'SQLITE_PASS\n'
```

关键输出：SQLite 3.48.0各测试项、`TOTAL`、`sqlite_exit=0`、`SQLITE_PASS`。固定`--size 10`完成且退出0为通过；不把单次运行耗时另作性能阈值。

### 4.3 2-3-0003：unsafe行数和比例

在目标 Host 本机执行，使用参与 Guest 构建的源码及仓库中的统计脚本。先按部署指南安装 cloc 和 Perl 依赖。输入：

```bash
(
  cd "$PLANB_BASE/asterinas"
  cloc --version
  sha256sum tools/count_unsafe_rust.py
  python3 tools/count_unsafe_rust.py --json > "$PLANB_LOGROOT/0003-unsafe.json"
)
python3 - "$PLANB_LOGROOT/0003-unsafe.json" <<'PY'
import json, sys
d = json.load(open(sys.argv[1]))
unsafe = d['unsafe_code_lines']
total = d['cloc']['rust_code_lines']
percentage = 100.0 * unsafe / total
print(f'unsafe={unsafe} rust_code={total} percentage={percentage:.6f}%')
assert unsafe <= 10000 and percentage <= 20
print('UNSAFE_THRESHOLDS_PASS')
PY
```

预期输出：`unsafe=<统计值> rust_code=<统计值> percentage=<统计值>%`；两个阈值均满足时输出 `UNSAFE_THRESHOLDS_PASS`。数值随实际源码变化，不预设固定结果。

保存脚本哈希、源码版本和 JSON 原始结果。通过阈值为 10000 行和 20%，分母为相同文件集合的有效 Rust 代码行；统计较慢时等待当前任务完成。

### 4.4 2-3-0005：CVM内存保护

输入：

```bash
./zion-runtime.sh linux
cat state/protected-addr
./zion-runtime.sh protect 2>&1 | tee "$PLANB_LOGROOT/0005-protect.log"
grep -q SIX_CASES_PASSED "$PLANB_LOGROOT/0005-protect.log" \
  && grep -q PRIVATE_CANARIES_OK "$PLANB_LOGROOT/0005-protect.log" \
  && printf 'MEMORY_PROTECTION_PASS\n'
```

关键输出：

```text
0x400080000
[PMP-LIVE VM0] POSITIVE_CONTROLS_OK ...
[PMP-LIVE VM0] read PASS monitor_cause=5 ... LINUX_CONTEXT_RESTORED
[PMP-LIVE VM0] write PASS monitor_cause=7 ... LINUX_CONTEXT_RESTORED
[PMP-LIVE VM0] execute PASS monitor_cause=1 ... LINUX_CONTEXT_RESTORED
[PMP-LIVE] PEER_MONITOR_DELTAS=1,1,1 PEER_POSITIVE_CONTROLS=7
[PMP-LIVE] SIX_CASES_PASSED VM0_LINUX_ALIVE VM1_LINUX_ALIVE PRIVATE_CANARIES_OK
[ZION MEMORY] PASS: host/peer accesses blocked and recovered; private canaries retained
MEMORY_PROTECTION_PASS
```

通过判据：合法访问对照成功，双向六项越权访问被真实PMP拦截，Linux上下文恢复，两域存活，测试数据保持。`state/protected-addr`是当前固定PMP测试ABI对应的实际VM1测试页，protect会从测试输出再次核验；它不冒充原tvm分配池地址。

0005大纲未限定Guest操作系统，当前测试对象是双Linux。若在Asterinas状态直接调用protect，兼容层会临时切Linux执行后恢复Asterinas，期间VM1 RAM会重载。

### 4.5 2-3-0006：tee-crypto

输入：

```bash
./zion-runtime.sh asterinas
ssh -p 10022 root@127.0.0.1 'sh -s' <<'GUEST' 2>&1 | tee "$PLANB_LOGROOT/0006-crypto.log"
set -eu
token=$(/usr/bin/tee-crypto enc mypassword 'hello CVM')
test "${#token}" -eq 608
plaintext=$(/usr/bin/tee-crypto dec mypassword "$token")
test "$plaintext" = 'hello CVM'
printf 'token_length=%s\nplaintext=%s\n' "${#token}" "$plaintext"
echo TEE_CRYPTO_ROUNDTRIP_PASS
if /usr/bin/tee-crypto dec wrongpassword "$token" > /tmp/0006-wrong; then exit 1; fi
test ! -s /tmp/0006-wrong
case "$token" in *0) changed="${token%?}1";; *) changed="${token%?}0";; esac
if /usr/bin/tee-crypto dec mypassword "$changed" > /tmp/0006-changed; then exit 1; fi
test ! -s /tmp/0006-changed
echo TEE_CRYPTO_AUTHENTICATION_REJECTION_PASS
GUEST
```

关键输出：`token_length=608`、`plaintext=hello CVM`、两个PASS标志。错误口令和改写token时的`invalid input or authentication failed`属于预期拒绝，且不应输出明文。608字符内容是加密封装token，不是裸AES密钥；示例口令仅作测试。

### 4.6 2-3-0007：tee-vault

输入：

```bash
./zion-runtime.sh asterinas
ssh -p 10022 root@127.0.0.1 'sh -s' <<'GUEST' 2>&1 | tee "$PLANB_LOGROOT/0007-vault.log"
set -eu
/usr/bin/tee-vault put acceptance-key '0123456789abcdef-PlanB-test-key'
value=$(/usr/bin/tee-vault get acceptance-key)
test "$value" = '0123456789abcdef-PlanB-test-key'
printf 'VAULT_READBACK=%s\n' "$value"
test "$(stat -c %a /run/tee-vault)" = 700
test "$(stat -c %a /run/tee-vault/.master)" = 600
test "$(stat -c %a /run/tee-vault/acceptance-key.gcm)" = 600
if /usr/bin/tee-vault get ../master; then exit 1; fi
echo TEE_VAULT_PUT_GET_AND_PERMISSIONS_PASS
GUEST
```

关键输出：读回输入值及`TEE_VAULT_PUT_GET_AND_PERMISSIONS_PASS`。通过判据为存取一致、目录700/文件600、非法路径被拒绝。vault使用Guest私有RAM，Guest重载后不保留，不表示跨重启硬件封存。

### 4.7 2-3-0008：受控S3篡改与一次性告警

先连接物理串口并开启本次Host启动的实际日志保存。输入：

```bash
./zion-runtime.sh tamper-s3 status | tee "$PLANB_LOGROOT/0008-before.log"
cat /proc/sys/kernel/random/boot_id | tee "$PLANB_LOGROOT/0008-boot-id.txt"
```

初值必须是`warnings=0`。若为1，停止本次首次告警重测；查看已有结果和执行其他用例不需要重启。需要新的首次告警窗口时由操作人员安排新Host启动，Guest重载和init不会清除告警。

确认初值和串口后输入：

```bash
./zion-runtime.sh tamper-s3 1 \
  && ./zion-runtime.sh linux 2>&1 | tee "$PLANB_LOGROOT/0008-linux-start.log"
ssh -p 10022 root@127.0.0.1 'sh -s' <<'GUEST' 2>&1 | tee "$PLANB_LOGROOT/0008-guest.log"
set -e
export PATH=/usr/sbin:/usr/bin:/sbin:/bin
uname -a
ifconfig eth0
dd if=/dev/vda of=/tmp/0008-blk-read bs=512 count=1
wc -c /tmp/0008-blk-read
GUEST
./zion-runtime.sh tamper-s3 status | tee "$PLANB_LOGROOT/0008-after.log"
./zion-runtime.sh status | tee "$PLANB_LOGROOT/0008-host.log"
```

Host后端日志应有一次真实`PLANB_INJECTED_S3 ... value=0x5a494f4e`。兼容状态应为`phase=consumed`、`zion_vcpu_tamper=0`、`warnings=1`。这里的变量名是兼容层状态，不是不存在的KVM sysfs参数。

物理串口应实际出现以下一对消息，时间戳和seq可变化：

```text
[ZION VCPU ALERT] hart=1 source=HOST-SHARED-CHANNEL register=S3 expected=0x0 observed=0x5a494f4e
[ZION VCPU ALERT] classification=SUSPECTED-TAMPER-OR-ABI-MISMATCH; private saved state is retained
```

继续读取，确认一次性消费和数据保持：

```bash
ssh -p 10022 root@127.0.0.1 'head -c 512 /dev/vda' > "$PLANB_LOGROOT/0008-vda.bin"
head -c 512 /run/planb-runtime/linux.img > "$PLANB_LOGROOT/0008-backing.bin"
cmp "$PLANB_LOGROOT/0008-vda.bin" "$PLANB_LOGROOT/0008-backing.bin" \
  && test "$(wc -c < "$PLANB_LOGROOT/0008-vda.bin")" -eq 512
./zion-runtime.sh tamper-s3 status | tee "$PLANB_LOGROOT/0008-after-second-read.log"
./zion-runtime.sh status | tee "$PLANB_LOGROOT/0008-host-final.log"
```

通过判据：实际注入值与本次物理串口一致，原始告警一对，计数0→1→1，后端只注入一次，SSH及512字节数据对照正常。保存Host启动ID、Host日志及实际串口采集文件；状态重打不能替代原始串口。

### 4.8 2-3-0009：virtio网络与块设备

输入：

```bash
./zion-runtime.sh linux
ssh -p 10022 root@127.0.0.1 'sh -s' <<'GUEST' 2>&1 | tee "$PLANB_LOGROOT/0009-virtio.log"
set -e
export PATH=/usr/sbin:/usr/bin:/sbin:/bin
uname -a
ifconfig eth0
test -b /dev/vda
cat /sys/class/net/eth0/device/modalias
cat /sys/block/vda/device/modalias
dd if=/dev/vda of=/tmp/blk-read bs=512 count=1
wc -c /tmp/blk-read
GUEST
ssh -p 10022 root@127.0.0.1 'head -c 512 /dev/vda' > "$PLANB_LOGROOT/0009-vda.bin"
head -c 512 /run/planb-runtime/linux.img > "$PLANB_LOGROOT/0009-backing.bin"
cmp "$PLANB_LOGROOT/0009-vda.bin" "$PLANB_LOGROOT/0009-backing.bin" \
  && test "$(wc -c < "$PLANB_LOGROOT/0009-vda.bin")" -eq 512 \
  && printf 'VIRTIO_BLOCK512_MATCH\n'
```

关键输出：`6.12.35-planb`、eth0为`UP RUNNING`且地址`10.0.3.15`、net类型`virtio:d00000001...`、blk类型`virtio:d00000002...`、512字节及`VIRTIO_BLOCK512_MATCH`。这证明基本网络/块数据通路，不作为外网/NAT或性能基准结论。

### 4.9 2-3-0010：Linux CVM内嵌Enclave

输入一，主测：

```bash
./zion-runtime.sh enclave
ssh -p 10022 root@127.0.0.1 \
  '/usr/bin/run-zion-enclave-demo; rc=$?; printf "demo_exit=%s\n" "$rc"; exit "$rc"' \
  2>&1 | tee "$PLANB_LOGROOT/0010-enclave-demo.log"
test "$(grep -c VERIFIED "$PLANB_LOGROOT/0010-enclave-demo.log")" -eq 10 \
  && grep -q 'PASS: all negative ABI tests' "$PLANB_LOGROOT/0010-enclave-demo.log" \
  && grep -q 'demo_exit=0' "$PLANB_LOGROOT/0010-enclave-demo.log" \
  && printf 'ENCLAVE_DEMO_PASS\n'
```

关键输出：十轮`VERIFIED`、`private checksum continuity across 10 OCALLs; destroyed`、`PASS: all negative ABI tests`、`demo_exit=0`及`ENCLAVE_DEMO_PASS`。共享缓冲区主动覆盖属于测试操作。

日志中的`LIMIT`说明demo未验证远程证明及私有页访问拒绝；`SKIP: arbitrary private-page access`不计作通过。需要复核设计文档中的隔离、上下文、销毁清零补充证据时，执行输入二：

```bash
ssh -p 10022 root@127.0.0.1 'sh -s' <<'GUEST' 2>&1 | tee "$PLANB_LOGROOT/0010-context-guard.log"
set -e
export PATH=/usr/sbin:/usr/bin:/sbin:/bin
/root/zion-enclave/planb-enclave-guard /root/zion-enclave/planb-context-probe.bin
insmod /root/zion-enclave/planb-context-audit.ko
trap 'rmmod planb_context_audit 2>/dev/null || true' EXIT
cat /sys/module/planb_context_audit/parameters/last_passed
test "$(cat /sys/module/planb_context_audit/parameters/last_passed)" = Y
/usr/bin/busybox dmesg | /usr/bin/busybox grep 'PLANB ENCLAVE AUDIT'
rmmod planb_context_audit
trap - EXIT
GUEST
```

补充预期：`PARENT_READ_DENIED`、`PARENT_WRITE_DENIED`、`PRIVATE_CONTEXT_PASS fp=32 rvv=1`、`DESTROY_ZERO_AND_STALE_HANDLE_PASS`、`Y`及`S_MODE_CONTEXT_PASS`。补充检查用于隔离与上下文审计，执行前须构建并装入所列探针及模块；不能将 SKIP 换算为 PASS。

## 5. 必须说明的平台差异

| 原参考环境 | 本方案处理 |
|---|---|
| `/dev/tvm`、`/dev/kvm`和tvm池初始化 | 使用真实m-ACRN服务ABI及固定隔离布局，不创建假设备、不模拟KVM存在 |
| KVM参数`/sys/module/kvm/parameters/zion_vcpu_tamper` | 使用`zion-runtime.sh tamper-s3`，将一次性请求交给真实方案B后端 |
| `kill $(cat state/cvm.pid)` | 使用原命令族已有的`zion-runtime.sh stop`，不伪造QEMU/Guest PID |
| QEMU的10022端口转发 | 由目标 Host 本机回环 SSH 代理连接运行的 VM1；SSH 命令名称保持一致 |
| 原Linux内核6.6.88+及密码认证 | 项目固定为 6.12.35-planb，使用安装时配置的本地公钥认证 |
| 原独立enclave镜像 | 当前Linux统一镜像同时具备virtio与Enclave；`enclave`保留为原命令别名 |

0004要求实体H扩展、KVM启动及HSTATUS/HGATP/HVIP/VSATP路径核验；SG2044不具备H扩展，仍不在九项覆盖范围内。0008、0009、0010按原大纲选Linux CVM；0005未限定Guest OS，当前选择双Linux。不能将这组证据写成Asterinas单独完成九项。

## 6. 保存本轮结果

测试时自行保存本轮日志、Host 启动 ID、源码与固件版本、用例编号及需要的物理串口证据。日志和凭据放在本地运行目录，不加入源码仓库。
