# r3测试说明

所有测试在SG2044的管理Linux本机及其Guest执行。先完成[安装](getting-started.md)。以下是输入和预期判据，不是某台机器已通过的成绩。

| 旧脚本编号 | 当前非H编号 | 项目 |
|---|---|---|
| 0001 | 0011 | Asterinas启动 |
| 0002 | 0012 | SQLite |
| 0003 | 0013 | Redis |
| 0004 | 0014 | MySQL |
| 0005 | 共用0005 | 固定源码TCB统计 |
| 0006 | 不适用 | 物理H扩展 |
| 0007 | 0016 | 内存保护 |
| 0008 | 0015 | vCPU共享通道篡改检测 |
| 0009 | 0017 | Linux网络与块读取 |
| 0010 | 0018 | CVM与独立Enclave |

`runtime/run-case.sh`每次执行一个旧编号并保存完整stdout、stderr和退出码JSON；数据库脚本位于`runtime/guest/`。从部署暂存目录调用时保留runtime目录结构。以下手动方法使用默认入口，自定义安装时对应替换路径。

## Host准备

```bash
cd /root/zion-tests
./zion-runtime.sh init
cat /proc/sys/kernel/random/boot_id
./zion-runtime.sh status --json
cat state/protected-addr
```

核对native_compat_abi=1、error=0及`/dev/tvm`。工具目录中的`tvm-control --info`提供abi、protected_addr、faults、tamper和used。物理串口单独保存M态原文，Host dmesg或Guest控制台不能替代它。

## 0011：Asterinas启动

Host执行：

```bash
./zion-runtime.sh select asterinas
sh ./zion-runtime.sh guest
./zion-runtime.sh status
ssh -p 10022 root@127.0.0.1
```

确认真实控制台的A图、Presented by the Asterinas developers和Released under Mozilla Public License v2.0。Guest执行`uname -a`和`busybox --help`，应有Linux zion-cvm及BusyBox 1.30.1。原始ANSI控制符保留，文本核对可对副本去色。

## 0012：SQLite

在Asterinas Guest执行并保存返回码及日志：

```sh
/benchmark/bin/sqlite-speedtest1 --size 10 2>&1 | tee 01-sqlite-speedtest1.log
```

要求SQLite 3.46.1，基本SQL操作正常、输出TOTAL且成功返回，无报错或卡死。计时为现场实测值，不要求重复某次历史数字。

## 0013：Redis

在已启动Asterinas的Host中，将实际脚本通过SSH stdin传给Guest：

```bash
ssh -p 10022 root@127.0.0.1 'sh -s' < "$ZION_STAGE/runtime/guest/test-redis.sh"
```

脚本源码为`runtime/guest/test-redis.sh`；也可由Host通过SSH stdin执行该文件。要求Redis 7.0.15、PING输出PONG，负载参数为`-n 100000 -c 20 -r 10000 -d 256 -t set,get -q`，SET和GET均输出吞吐和延迟汇总。脚本单独校验benchmark退出码，正常关闭测试实例；保留`redis-benchmark.log`和服务日志。

## 0014：MySQL

在Host执行下列命令，将实际脚本传给Asterinas Guest：

```bash
ssh -p 10022 root@127.0.0.1 'sh -s' < "$ZION_STAGE/runtime/guest/test-mysql.sh"
```

它按`/etc/zion-mysql.cnf`初始化隔离测试目录、等待服务就绪、建立本机测试账号并执行官方mysqlslap。版本应为8.0.46，缓冲池33554432字节、native AIO=0；负载为mixed、并发2、轮数2、查询200，正常输出平均/最小/最大耗时、2客户端、平均每客户端100查询。脚本校验独立负载退出码，保存mysql-benchmark.log并清理测试服务。

## 0016：内存保护

开启物理串口保存后，在已初始化的Host执行：

```bash
cat state/protected-addr
./zion-runtime.sh protect 2>&1 | tee 01-unauthorized-access.log
```

保留大纲原命令形式。保存命令结果、实际保护地址和同次串口。M态应输出`[SM] TEE security check: the hypervisor is trying to r/w the protected region`及带物理拦截地址的拒绝信息；驱动应输出`protected-memory read blocked: physical address=... fault=-14 (recovered)`。核对地址一致、真实访问未返回私有数据、故障计数增加且Host恢复。该项可以重复，不要求为每次越权读重启Host。

## 0015：共享S3篡改检测

这项每次Host启动只有一个告警窗口。物理串口已开始保存，init成功且`/dev/tvm`存在；先确认used=0、warnings=0、参数=0。窗口已使用时由操作人员安排重启；重载模块不能重置monitor状态。

Host执行一次：

```bash
cat /sys/module/kvm/parameters/zion_vcpu_tamper
printf '1\n' > /sys/module/kvm/parameters/zion_vcpu_tamper
./zion-runtime.sh linux
cat /sys/module/kvm/parameters/zion_vcpu_tamper
```

要求Host事件`register=S3 value=0x5a494f4e`，物理串口一次事件共两行：

```text
[ZION VCPU ALERT] hart=1 source=HOST-SHARED-CHANNEL register=S3 expected=0x0 observed=0x5a494f4e
[ZION VCPU ALERT] classification=SUSPECTED-TAMPER-OR-ABI-MISMATCH; private saved state is retained
```

参数由1归0、used=1、warnings=1。随后按0017执行SSH和512字节读取，两项均应成功。保存同次Host命令、串口和消费状态；不要为补日志再次置1。

## 0017：Linux、网络与块读取

Host执行`./zion-runtime.sh linux`后，通过`ssh -p 10022 root@127.0.0.1`进入Guest：

```sh
uname -a
ifconfig eth0
dd if=/dev/vda of=/tmp/blk-read bs=512 count=1
wc -c /tmp/blk-read
```

要求参考内核Linux zion-cvm 6.6.88+ riscv64、eth0为UP、读取成功且文件大小512。MAC和设备计数按现场值记录。

## 0018：独立Enclave

Host执行`./zion-runtime.sh stop`、`./zion-runtime.sh enclave`，确认runtime image=3后SSH登录。此模式没有`/dev/vda`、相应sysfs或FDT块节点。Guest运行：

```sh
/usr/bin/run-zion-enclave-demo; rc=$?; printf 'demo_exit=%s\n' "$rc"
```

十轮COMPUTE均VERIFIED，私有checksum跨OCALL连续，销毁成功，DRIVER SECURITY负向ABI项全部PASS、demo_exit=0。SKIP不计通过；保留程序的LIMIT行。完成后受控停止并按需要切回Linux或Asterinas。

## 共用0005：TCB统计

该项不按平台重复设置。在源码主机固定`sdww0/asterinas`的`536593f341c581d430f6743cfd322c09e4e0be26`，使用cloc 2.02：

```bash
cloc --version
python3 tools/count_unsafe_rust.py
```

保存源码标识、脚本、完整输出和结果截图。unsafe≤10000行且占比≤20%。若统计本仓库实际移植树，应如实注明统计对象，不能把固定上游快照的数字填作实际移植树结果。`run-case.sh 0005`通过`ZION_STATS_SOURCE`明确指定统计目录。

## 留存

每项保存当前编号、原脚本编号、UTC日期、Host boot ID、实际命令、程序退出码、完整stdout/stderr、必要串口及结论。原件与去色展示副本分开保存。独立测例无需以同次Host完成全部测试作为额外验收条件。
