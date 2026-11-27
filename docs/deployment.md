# 部署与运行

## 环境准备

- SG2044管理Linux已由配套启动镜像启动，安全监控器提供原生ABI 1。
- Host工具和驱动与启动镜像匹配。
- Guest已配置SSH公钥，Host持有对应私钥。

已有`/root/zion-tests`入口的环境可直接[启动Guest](#启动guest)。首次安装需要准备以下文件：

```text
planb-guestctl
planb-backend
macrn-pmp-test
planb-runtime.sh
tvm-control
tvm-driver.ko
kvm.ko
guest-test-client.pub
```

Host上的`kvm.ko`提供控制参数接口，Guest运行由安全监控器管理。

## 首次安装Host工具

将工具和公钥放入同一目录，生成校验清单：

```bash
cd /path/to/host-tools
sha256sum planb-guestctl planb-backend macrn-pmp-test planb-runtime.sh \
  tvm-control tvm-driver.ko kvm.ko guest-test-client.pub > SHA256SUMS
```

在本仓库根目录，以root执行：

```bash
python3 host/install-host.py --tools-dir /path/to/host-tools \
  --key /path/to/guest_ed25519 --prefix /opt/sg2044 \
  --entrypoint /root/zion-tests
```

私钥应由root持有、权限为0600，并与Guest中的授权公钥匹配。安装程序会检查ABI和输入文件、保存SSH配置备份；已有安装入口会拒绝覆盖。

## 启动Guest

在管理Host上执行：

```bash
cd /root/zion-tests
./zion-runtime.sh init
./zion-runtime.sh status --json
./zion-runtime.sh select asterinas
sh ./zion-runtime.sh guest
ssh -p 10022 root@127.0.0.1
```

`init`初始化驱动、校验工具并建立访问入口。状态中的`native_compat_abi`应为1，`error`应为0。

```bash
./zion-runtime.sh linux
./zion-runtime.sh enclave
./zion-runtime.sh stop
```

上述命令分别启动Linux CVM、启动独立Enclave环境和停止Guest。切换会清理Guest私有内存；独立Enclave环境不提供virtio块设备。

## Asterinas内核

仓库提供[预编译内核](../prebuilt/asterinas/README.md)，可直接作为统一载荷的内核输入。配套initrd、SHA256和来源提交见[内核清单](../prebuilt/asterinas/manifest.json)。

完整SD镜像包含启动载荷和前级启动文件。更换单个内核时，需先更新`SG2044.fd`，再重新上电加载。

## Linux CVM内核

使用现成的Linux 6.6.88+内核及配套模块、initramfs，核对文件：

```bash
python3 tools/reference-kernel.py --kernel /path/to/guest_kernel_image
```

从统一载荷中提取并核对：

```bash
python3 tools/reference-kernel.py --payload /path/to/riscv64/SG2044.fd --output /tmp/guest_kernel_image
```

内核应为28683264字节，并匹配[kernel-reference.json](../kernel-reference.json)中的SHA256。
