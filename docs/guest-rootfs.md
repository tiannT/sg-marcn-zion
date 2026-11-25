# r3 Guest用户态与初始化

r3以外部提供的兼容参考initramfs为输入。所有BusyBox、SSH、SQLite、Redis、MySQL及其动态库均在构建时作为CPIO条目内置；Guest启动时校验程序并启动SSH，数据库服务由测例显式启动。

准备本机生成的Guest客户端公钥`inputs/guest-client.pub`。私钥留在目标Host，不进入Guest或仓库。参考SSH配置可能将root的home设为`/`，转换器按此保存`/.ssh/authorized_keys`；Linux转换器同时保存`/root/.ssh/authorized_keys`。已有SSH服务Host key由使用者独立管理。

## Asterinas

参考输入必须包含以下真实程序及完整运行依赖：

| 路径 | 版本/用途 |
|---|---|
| `/usr/bin/busybox` | BusyBox 1.30.1 |
| `/usr/sbin/dropbear`、`/etc/zion-ssh.sh` | 参考SSH与网络启动配置 |
| `/benchmark/bin/sqlite-speedtest1` | SQLite 3.46.1 |
| `/usr/local/redis/bin/redis-server`、`redis-cli`、`redis-benchmark` | Redis 7.0.15 |
| `/usr/sbin/mysqld`、`/usr/bin/mysql`、`mysqladmin`、`mysqlslap` | MySQL 8.0.46 |
| `/etc/zion-mysql.cnf` | 32MiB InnoDB缓冲池、关闭native AIO；示例见`configs/zion-mysql.cnf` |

在构建机仓库根目录执行：

```bash
python3 scripts/prepare-reference-asterinas.py   --reference inputs/reference/initrd-asterinas.img   --authorized-key inputs/guest-client.pub   --out-dir build/asterinas-reference
```

输出`build/asterinas-reference/asterinas-matched.cpio.gz`作为组装器的`--asterinas-initrd`。脚本生成`/etc/planb-reference-files.sha256`，逐项验证原程序字节未变，启动入口校验后执行原`/etc/zion-ssh.sh`。内核Image需来自匹配的Asterinas移植构建；`uname`为应用兼容返回Linux zion-cvm，运行系统以真实启动横幅确认。

## Linux CVM与Enclave

提供Linux 6.6.88+参考内核及其匹配的initramfs。initramfs需包含SSH、virtio设备支持、`/usr/bin/run-zion-enclave-demo`及其所需驱动、SDK/runtime/loader和应用。保留完整原依赖，不能将其他内核的模块直接装入。

```bash
python3 scripts/prepare-reference-linux.py   --reference inputs/reference/initrd-linux.img   --authorized-key inputs/guest-client.pub   --pmp-helper build/tools/macrn-pmp-test   --out-dir build/linux-reference
```

输出`build/linux-reference/linux-matched.cpio.gz`作为`--linux-initrd`。原生初始化入口挂载基础文件系统、启动PMP辅助服务，再执行原`/etc/zion-ssh.sh`。保留原Enclave程序和驱动字节并记录哈希，不改写它们的测试结果。

同一Linux内核/initramfs可用于image2和image3：由M态FDT与后端配置决定是否提供块设备。image2必须有`/dev/vda`，image3必须没有；初始化不能强制要求两种模式都有vda。两种模式Guest网络均为`10.0.2.15/24`，Host本机入口为`127.0.0.1:10022`。

普通Linux与Asterinas的RAM数据会随受控停止清理；重载从相同initramfs恢复内置程序。转换器输出及私有公钥配置保留在`build/`或`inputs/`中，不加入源码Git。
