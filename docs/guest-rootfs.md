# Guest rootfs 准备

统一载荷组装器接受已经构建的 RISC-V Image 与 initramfs。本说明列出文件布局；用户需准备所选 OS 支持的 RISC-V 用户态与运行库，不能使用构建机的 x86 程序代替。

## 公共要求

提供 shell、基础工具、SSH 服务及其完整运行库。将选择的公钥加入 `/root/.ssh/authorized_keys`，目录权限 700、文件权限 600；为 Guest SSH 服务独立生成 Host key。初始化入口、脚本解释器和内核参数须一致。Nix 构建的程序还需完整 ELF 解释器和依赖闭包，不能仅设置 `LD_LIBRARY_PATH`。

## Asterinas

| Guest 路径 | 输入 |
|---|---|
| `/init-planb-asterinas` | `runtime/guest/asterinas-init.sh`，可执行 |
| `/test/planb-asterinas-ssh.sh` | `runtime/guest/asterinas-services.sh` |
| `/bin/sh` | 受支持的 shell 与依赖 |
| `/usr/sbin/dropbear` | RISC-V Dropbear 和运行库 |
| `/etc/dropbear/host_key` | 独立生成的 Dropbear Host key |
| `/benchmark/bin/sqlite-speedtest1` | SQLite speedtest1 |
| `/usr/bin/tee-crypto`、`/usr/bin/tee-vault` | 项目应用与依赖 |
| `/root/.ssh/authorized_keys` | 与 Host 本地身份匹配的公钥 |

启动参数选择 `/init-planb-asterinas`。服务脚本使用 Guest 地址 `10.0.2.15/24`；该地址属于项目虚拟网络，不是目标服务器管理地址。

## Linux CVM

| Guest 路径 | 输入 |
|---|---|
| `/init-planb` | `runtime/guest/linux-init.sh`，可执行 |
| `/bin/bash` | Bash 及运行库 |
| `/usr/sbin/sshd`、`/etc/ssh/sshd_config` | OpenSSH、配置、Host key 及依赖 |
| `/usr/bin/macrn-pmp-test` | `build/tools/macrn-pmp-test` |
| `ip`、`ifconfig`、`mount`、`dd`、`wc`、`head` 等 | RISC-V 基础用户态 |
| `/usr/bin/run-zion-enclave-demo` | `src/zion-enclave/test/run-nested-demo.sh`，可执行 |
| `/root/zion-enclave/zion-driver.ko` | 针对本次 Linux 内核构建的驱动 |
| `/root/zion-enclave/demo`、`demo-runner` | `build/zion/examples/` 的同名输出 |
| `/root/zion-enclave/eyrie-rt`、`loader.bin` | `build/zion/runtime-output/` 的同名输出 |
| `/root/zion-enclave/zion-driver-security` | `build/zion/zion-driver-security` |
| `/root/.ssh/authorized_keys` | 与 Host 身份匹配的公钥 |

使用 `rdinit=/init-planb`。项目配置将 virtio 驱动编入内核，初始化不需要 `/test-modules/`。Enclave 驱动模块必须针对同一次 Linux 内核构建。

OpenSSH 配置须启用 root 公钥登录，并指定 Guest 内实际存在的 Host key。例如在隔离测试 Guest 的 `sshd_config` 中使用 `PermitRootLogin prohibit-password`、`PubkeyAuthentication yes` 和 `PasswordAuthentication no`。初始化脚本会启动 PMP 对等测试服务，再以前台方式启动 sshd。

Host 与 Linux Guest 的直连地址为 `10.0.3.1/24`、`10.0.3.15/24`。功能测试使用该直连网络；外网、NAT 和默认网关另行配置。

## 打包

准备完整文件树后，在构建机以 newc 格式打包。以下变量指向本地构建目录，输出必须位于文件树之外：

```bash
ROOTFS_DIR="$PROJECT_DIR/build/rootfs-linux"
OUTPUT_INITRAMFS="$PROJECT_DIR/inputs/linux/initramfs.cpio.gz"
mkdir -p "$(dirname "$OUTPUT_INITRAMFS")"
(
  cd "$ROOTFS_DIR"
  find . -print0 | cpio --null -o --format=newc --owner=0:0
) | gzip -n > "$OUTPUT_INITRAMFS"
```

打包前检查文件、解释器、库和权限。构建镜像及 Guest Host key 留在本地输出目录，不加入源码仓库。
