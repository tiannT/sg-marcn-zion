# 首次部署r3

本指南面向新用户。构建机准备源码和输入，目标管理Host本机执行控制及测试；烧录、Host重启和串口记录由操作人员安排。

## 1. 获取源码与环境

```bash
git clone https://github.com/tiannT/sg-marcn-zion.git
cd sg-marcn-zion
export PROJECT_DIR="$PWD"
python3 -B scripts/check-source.py
python3 -B -m unittest discover -s runtime/zion-compat -p 'test_*.py'
```

构建机需Bash、Python3.10+、Make、RISC-V GCC/binutils、静态libc开发文件和device-tree-compiler；目标Host需root、Python3.10+、OpenSSH客户端、TUN/TAP、`/dev/mem`、iproute、util-linux和kmod。Asterinas、Enclave源码构建另需其工具链。

准备VM0基础系统、匹配的Asterinas内核和参考initramfs、Linux6.6.88+参考内核和匹配的initramfs、板级DTB与前级启动组件。它们是外部输入，不能由一个源码检查命令替代。Linux6.12.35子模块属于可选示例，r3参考路径无需初始化它。

## 2. Guest客户端身份

在目标Host的root Bash生成本地身份，已有身份按实际部署沿用：

```bash
install -d -m 700 /etc/sg-marcn-zion
if [ ! -e /etc/sg-marcn-zion/guest_ed25519 ]; then
  ssh-keygen -q -t ed25519 -N '' -f /etc/sg-marcn-zion/guest_ed25519
fi
chmod 600 /etc/sg-marcn-zion/guest_ed25519
ssh-keygen -y -f /etc/sg-marcn-zion/guest_ed25519 > /etc/sg-marcn-zion/guest_ed25519.pub
```

仅将公钥传到构建机`inputs/guest-client.pub`；私钥保留目标Host。参考Guest自己的SSH Host key需独立管理。

## 3. 构建工具与Host模块

在构建机仓库根目录执行：

```bash
export CROSS_COMPILE=riscv64-linux-gnu-
bash scripts/build-monitor.sh
bash scripts/build-tools.sh
read -r -p 'Configured target Host kernel build directory: ' KERNEL_BUILD_DIR
export KERNEL_BUILD_DIR
bash scripts/build-host-driver.sh
```

`KERNEL_BUILD_DIR`必须是目标管理Linux完全匹配的已配置构建目录，含`.config`、真实`Module.symvers`及生成头文件。交付参考Host是6.12.32-0.0.0.0.riscv64；不得用Guest内核目录代替。模块不支持强制加载或伪造符号CRC。

输出为`build/monitor/fw_dynamic.bin`及`build/tools/`下的控制器、I/O后端、PMP辅助程序、tvm-control、两个Host模块及运行脚本。

## 4. 准备Guest并组装载荷

按[Guest rootfs说明](guest-rootfs.md)运行参考initramfs转换器，生成Asterinas与Linux的直接内置版本。准备三个域各自的单行cmdline.txt，然后执行：

```bash
python3 scripts/package-firmware.py   --vm0-kernel inputs/vm0/Image --vm0-initrd inputs/vm0/initramfs.cpio.gz   --vm0-cmdline-file inputs/vm0/cmdline.txt   --asterinas-kernel inputs/asterinas/Image   --asterinas-initrd build/asterinas-reference/asterinas-matched.cpio.gz   --asterinas-cmdline-file inputs/asterinas/cmdline.txt   --linux-kernel inputs/linux/Image   --linux-initrd build/linux-reference/linux-matched.cpio.gz   --linux-cmdline-file inputs/linux/cmdline.txt   --board-dtb inputs/board.dtb --firmware build/monitor/fw_dynamic.bin   --output-dir "dist/r3-$(date -u +%Y%m%dT%H%M%SZ)"
```

要求板级DTB为r3使用的SG2044原生ISA树。组装器去除残留QEMU扩展列表，保留0x88000000的启动FDT槽并检查254MiB加载上限，输出配对的fw_dynamic.bin和SG2044.fd。SG2044.fd是项目统一载荷，不是通用UEFI固件。每个新输出注入独立实验种子，不复用旧输出目录；它不构成认证启动链或生产熵源。

由操作人员将两份文件和匹配的前级组件放入实际启动介质，重启并保存串口。不要把管理Linux的/boot/efi自动当作实际启动卡。

## 5. 安装Host入口

在构建机准备可传送目录，保持runtime目录结构：

```bash
mkdir -p build/host-package/tools
cp -a build/tools/. build/host-package/tools/
cp -a runtime build/host-package/runtime
cp inputs/guest-client.pub build/host-package/tools/guest-test-client.pub
(
  cd build/host-package/tools
  sha256sum planb-guestctl planb-backend macrn-pmp-test planb-runtime.sh     tvm-control tvm-driver.ko kvm.ko guest-test-client.pub > SHA256SUMS
)
```

将该目录传至目标Host的暂存位置。在目标root Bash安装到自选工具目录，以下使用默认配置：

```bash
read -r -p 'Transferred host-package directory: ' ZION_STAGE
install -d -m 700 /opt/sg-marcn-zion-tools
cp -a "$ZION_STAGE/tools/." /opt/sg-marcn-zion-tools/
python3 "$ZION_STAGE/runtime/zion-compat/install.py"   --tools-dir /opt/sg-marcn-zion-tools   --prefix /opt/sg-marcn-zion   --key /etc/sg-marcn-zion/guest_ed25519   --entrypoint /root/zion-tests
cd /root/zion-tests
./zion-runtime.sh init
./zion-runtime.sh status --json
```

安装器要求原生ABI1及本机Guest身份，安装前备份相关原文件，已有入口拒绝覆盖；不修改SSH服务端或固件。init核验哈希后加载匹配模块、查询真实保护地址和参数接口。路径可通过参数调整，见`configs/deployment.env.example`。已有r3安装应核对版本和备份后由维护人员安排更新，不能盲目再次运行首次安装器。

## 6. 测试

按[测试说明](testing.md)执行。手动命令在Host/Guest对应位置运行，物理串口另行保存。也可从保留完整runtime结构的暂存目录执行单个旧编号：

```bash
ZION_ENTRYPOINT=/root/zion-tests bash "$ZION_STAGE/runtime/run-case.sh" 0002
```

该示例是SQLite，对应当前0012；不要将新0012直接传给旧编号脚本。篡改0008只在串口记录与未消费窗口均就绪时执行。所有现场输出保存在目标Host，源码上传不包含现场日志。
