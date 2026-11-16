# 首次部署指南

本指南面向首次使用本项目的用户。流程为：获取源码、准备构建环境和 Guest 身份、构建组件、组装并加载固件、安装 Host 工具与兼容层、启动 Guest、执行测试。

## 1. 前置条件

| 位置 | 条件与用途 |
|---|---|
| 构建机 | Linux 或 WSL、Bash、Git、Python 3.10+、Make、RISC-V GCC/binutils；用于编译和组装镜像 |
| 目标 Host | SG2044 管理 Linux，root 权限、Python 3.10+、OpenSSH 客户端、iproute、util-linux、kmod、tar、gzip；运行控制与 I/O 后端 |
| 板级操作 | 可访问物理串口，能够确认启动介质并手动加载固件 |
| Guest 输入 | 与项目配置匹配的内核、initramfs、运行库和所需测试程序 |

目标 Host 需支持 TUN/TAP，并允许本项目工具访问 `/dev/mem`。Guest 登录和功能测试全部在目标 Host 本机完成，不使用构建机 SSH agent 转发。

仓库提供源码、配置和构建入口；工具链、VM0 基础系统及 Guest 用户态需要另行构建或准备。下文明确列出输入，不假定用户已经具有某个内部环境或镜像。

## 2. 获取源码和上游依赖

在构建机从本仓库页面复制克隆地址：

```bash
read -r -p 'Repository URL: ' REPOSITORY_URL
git clone "$REPOSITORY_URL" sg-marcn-zion
cd sg-marcn-zion
export PROJECT_DIR="$PWD"
bash scripts/init-submodules.sh
python3 -B scripts/check-source.py
```

Linux 子模块固定到 `v6.12.35` 的提交 `783cd2c3dca8b6c434e955b84c20c8940588dc68`。首次获取需要网络，主仓库只保存引用及配置。

Ubuntu 构建机的基础依赖示例：

```bash
sudo apt-get update
sudo apt-get install -y build-essential gcc-riscv64-linux-gnu \
  g++-riscv64-linux-gnu cmake binutils-riscv64-linux-gnu \
  libc6-dev-riscv64-cross python3 git \
  rsync curl device-tree-compiler cpio xz-utils flex bison bc \
  libssl-dev libelf-dev
```

Asterinas、Enclave 和应用另有 Rust、开发容器、SDK 或 Nix 依赖，见[构建说明](build.md)。本地单元检查不会启动 Guest：

```bash
PYTHONDONTWRITEBYTECODE=1 python3 -B runtime/zion-compat/test_compat.py
PYTHONDONTWRITEBYTECODE=1 python3 -B runtime/zion-compat/test_install.py
```

## 3. 确定部署配置和 Guest 身份

默认值如下，均可通过安装参数调整：

| 参数 | 默认值 | 用途 |
|---|---|---|
| `--prefix` | `/opt/sg-marcn-zion` | 兼容层、配置与运行状态 |
| `--tools-dir` | `/opt/sg-marcn-zion-tools` | 运行工具与哈希清单 |
| `--key` | `/etc/sg-marcn-zion/guest_ed25519` | Host 本地 Guest 身份 |
| `--entrypoint` | `/root/zion-tests` | 原 Zion 操作形式的入口 |
| `--backup-dir` | `/var/backups/sg-marcn-zion` | 安装时的原文件备份 |

配置示例见 [`configs/deployment.env.example`](../configs/deployment.env.example)。以下使用默认值；自定义时，后续命令使用同一组配置。工具目录应独立于兼容层前缀，私钥应位于工具目录之外。

在目标 Host 的 root Bash 中生成独立身份；已存在的文件不会被重建：

```bash
install -d -m 700 /etc/sg-marcn-zion
if [ ! -e /etc/sg-marcn-zion/guest_ed25519 ]; then
  ssh-keygen -q -t ed25519 -N '' -f /etc/sg-marcn-zion/guest_ed25519
fi
chmod 600 /etc/sg-marcn-zion/guest_ed25519
ssh-keygen -y -f /etc/sg-marcn-zion/guest_ed25519 \
  > /etc/sg-marcn-zion/guest_ed25519.pub
```

在构建机指定目标地址并取得公钥：

```bash
read -r -p 'Target Host address: ' TARGET_HOST
mkdir -p inputs
ssh root@"$TARGET_HOST" 'cat /etc/sg-marcn-zion/guest_ed25519.pub' \
  > inputs/guest-client.pub
```

将公钥加入两种 Guest rootfs 的 `/root/.ssh/authorized_keys`，目录权限 700、文件权限 600。私钥留在目标 Host，不放入 Guest 镜像或 Git。Guest SSH 服务的 Host key 也应独立生成。

## 4. 编译与准备 Guest 文件树

在构建机仓库根目录执行：

```bash
export CROSS_COMPILE=riscv64-linux-gnu-
bash scripts/build-monitor.sh
bash scripts/build-tools.sh
bash scripts/build-linux-cvm.sh
```

| 输出 | 内容 |
|---|---|
| `build/monitor/fw_dynamic.bin` | M-mode 固件 |
| `build/tools/` | Host 控制工具、I/O 后端、PMP 工具及运行脚本 |
| `build/linux-cvm/arch/riscv/boot/Image` | Linux CVM 内核 |

按[构建说明](build.md)编译 Asterinas、Enclave 和应用。内核模块必须针对所使用的 Linux 内核构建。

按 [Guest rootfs 准备说明](guest-rootfs.md)组装 initramfs。必须提供 RISC-V shell、SSH 服务、正确的初始化入口、授权公钥、运行库及对应测试程序。仅构建内核不能替代用户态准备。

## 5. 组装并加载统一固件

准备输入文件：

| 输入 | 要求 |
|---|---|
| VM0 Image、initramfs、启动参数 | 能在目标板启动管理 Linux，匹配其根文件系统和设备 |
| Asterinas Image、initramfs、启动参数 | 初始化入口和运行库完整，包含所需应用 |
| Linux CVM Image、initramfs、启动参数 | SSH、virtio、PMP 服务及需要的 Enclave 组件齐全 |
| 板级 DTB | 完整有效的 SG2044 设备树，包含 `/chosen` |
| 前级启动组件 | 与目标板匹配的 ZSBL、FSBL、启动配置等 |

每个 `cmdline.txt` 包含一行完整启动参数。将本地输入放入忽略目录 `inputs/`，准备后执行：

```bash
python3 scripts/package-firmware.py \
  --vm0-kernel inputs/vm0/Image \
  --vm0-initrd inputs/vm0/initramfs.cpio.gz \
  --vm0-cmdline-file inputs/vm0/cmdline.txt \
  --asterinas-kernel inputs/asterinas/Image \
  --asterinas-initrd inputs/asterinas/initramfs.cpio.gz \
  --asterinas-cmdline-file inputs/asterinas/cmdline.txt \
  --linux-kernel build/linux-cvm/arch/riscv/boot/Image \
  --linux-initrd inputs/linux/initramfs.cpio.gz \
  --linux-cmdline-file inputs/linux/cmdline.txt \
  --board-dtb inputs/board.dtb \
  --firmware build/monitor/fw_dynamic.bin \
  --output-dir "dist/planb-$(date +%Y%m%dT%H%M%S)"
```

输出为 `fw_dynamic.bin` 和 `SG2044.fd`，后者是本项目自定义统一载荷，不是通用 UEFI 镜像。组装入口要求使用新输出目录，并生成新的单次启动实验种子。

由操作人员按板卡启动介质布局加载两份文件，保留匹配的前级组件，启动并保存串口输出。不要将管理系统的 `/boot/efi` 自动视为测试介质。项目脚本不会自动烧录或重启板卡。

确认管理 Linux 可以登录，并在目标 root Bash 检查：

```bash
command -v python3 ssh ssh-keygen ss ip flock stat sha256sum modprobe tar
test -c /dev/mem
```

缺少命令时使用目标发行版包管理器安装相应依赖；TUN 为模块时加载目标内核提供的 `tun`。

## 6. 部署 Host 工具和兼容层

构建机准备安装输入并传送：

```bash
mkdir -p build/host-package/tools build/host-package/zion-compat
cp -a build/tools/. build/host-package/tools/
cp -a runtime/zion-compat/. build/host-package/zion-compat/
cp configs/deployment.env.example build/host-package/deployment.env
cp inputs/guest-client.pub build/host-package/tools/guest-test-client.pub
(
  cd build/host-package/tools
  sha256sum planb-guestctl planb-backend macrn-pmp-test planb-runtime.sh \
    guest-test-client.pub > SHA256SUMS
)
ssh root@"$TARGET_HOST" 'install -d -m 700 /var/lib/sg-marcn-zion-stage'
scp -r build/host-package/. root@"$TARGET_HOST":/var/lib/sg-marcn-zion-stage/
```

在目标 Host root Bash 中核对配置后首次安装：

```bash
cd /var/lib/sg-marcn-zion-stage
source ./deployment.env
(
  set -eu
  test ! -e "$ZION_PREFIX"
  test ! -e "$ZION_TOOLS_DIR"
  install -d -m 700 "$ZION_TOOLS_DIR"
  for name in planb-guestctl planb-backend macrn-pmp-test planb-runtime.sh; do
    install -m 700 "tools/$name" "$ZION_TOOLS_DIR/$name"
  done
  install -m 600 tools/guest-test-client.pub tools/SHA256SUMS "$ZION_TOOLS_DIR/"
  (cd "$ZION_TOOLS_DIR" && sha256sum -c SHA256SUMS)
  "$ZION_TOOLS_DIR/planb-guestctl" --status
  python3 zion-compat/install.py \
    --prefix "$ZION_PREFIX" --tools-dir "$ZION_TOOLS_DIR" \
    --key "$ZION_GUEST_KEY" --entrypoint "$ZION_ENTRYPOINT" \
    --backup-dir "$ZION_BACKUP_DIR"
)
```

校验清单名是 `SHA256SUMS`。哈希用于确认传输文件，不能替代来源和固件 ABI 的核对。状态检查必须成功后再安装。

兼容层配置保存在 `--prefix` 下；SSH 身份仅对本机 `127.0.0.1:10022` 生效。安装程序拒绝覆盖已有入口；目录已存在时先核对部署配置，不继续执行首次安装命令。

## 7. 启动并检查 Guest

在目标 root Bash 中设置安装前缀，自定义安装时使用实际值：

```bash
export ZION_PREFIX=/opt/sg-marcn-zion
cd "$ZION_PREFIX"
./zion-runtime.sh init
./zion-runtime.sh select asterinas
sh ./zion-runtime.sh guest
ssh -p 10022 root@127.0.0.1 'id; cat /proc/version'
```

预期：初始化返回 `READY`，Guest 返回 `GUEST_SSH_READY`，SSH 能查询身份和包含 Rust 编译器信息的版本。`select` 只保存选择，`guest` 执行启动和就绪检查。

检查 Linux CVM：

```bash
./zion-runtime.sh linux
ssh -p 10022 root@127.0.0.1 'uname -a; ifconfig eth0; test -b /dev/vda'
```

预期：内核版本与构建版本一致，网卡为 `UP`，块设备检查返回 0。用 `./zion-runtime.sh status --json` 查询实际状态。

跨模式切换会清理并重载 VM1 RAM，临时文件和内存密钥库随之清除。两种 Guest 共用 Host 本机 10022 登录入口。

## 8. 准备源码统计

0003 在目标 Host 本机执行，需要 Python、Perl、cloc 及 `Algorithm::Diff`、`Regexp::Common` 模块。可使用目标发行版仓库或 CPAN 安装模块。以下固定 cloc 1.90：

```bash
install -d /usr/local/bin
curl -fL https://github.com/AlDanial/cloc/releases/download/v1.90/cloc-1.90.pl \
  -o /usr/local/bin/cloc
chmod 755 /usr/local/bin/cloc
perl -MAlgorithm::Diff -MRegexp::Common -e 'print "PERL_DEPS_OK\n"'
cloc --version
```

在构建机将实际参与 Guest 构建的 Rust 源码传到目标：

```bash
ssh root@"$TARGET_HOST" 'install -d /opt/sg-marcn-zion-source/asterinas/tools'
rsync -a src/asterinas/kernel src/asterinas/ostd \
  root@"$TARGET_HOST":/opt/sg-marcn-zion-source/asterinas/
scp src/asterinas/tools/count_unsafe_rust.py \
  root@"$TARGET_HOST":/opt/sg-marcn-zion-source/asterinas/tools/
```

自定义统计目录时，在测试准备中设置对应的 `PLANB_BASE`。依赖缺失或源码不匹配时先修复环境。

## 9. 执行测试

按[测试手册](testing.md)创建本次日志目录，逐项执行命令、检查退出码和输出。手册提供预期行为与判据，不预设任何用户环境已经通过。

可按 Linux 组 `0008 → 0005 → 0009 → 0010`、Asterinas 组 `0001 → 0002 → 0006 → 0007` 组织，0003 单独统计。0008 需要实际串口证据和可用的首次告警窗口；0004 需要实体 H 扩展，不能用本项目无 H 路径替代。

构建镜像、凭据、安装状态、日志和用户测试结果保存在本地，不加入 Git。
