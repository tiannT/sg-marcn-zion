# R3部署与运行

以现有R3交付包及其配套身份为输入。已部署的Host可直接运行原Zion命令，无需重复安装。更换固件和Host重启由操作人员安排。

## 核对参考内核

使用天权星现成guest_kernel_image：

```bash
python3 tools/reference-kernel.py --kernel /path/to/guest_kernel_image
```

如只有R3交付包，可从riscv64/SG2044.fd提取并核对原内核：

```bash
python3 tools/reference-kernel.py --payload /path/to/R3/riscv64/SG2044.fd --output /tmp/guest_kernel_image
```

两种输入都必须匹配kernel-reference.json中的28683264字节及SHA256。提取仅复制已有字节，不编译或重新组装镜像。Guest模块与initramfs沿用该内核的R3配套文件。

## Host文件

使用R3交付包linux目录的planb-guestctl、planb-backend、macrn-pmp-test、planb-runtime.sh、tvm-control、tvm-driver.ko和kvm.ko。保留交付包对应的guest-test-client.pub，以及与Guest授权匹配的Host本机私钥；不要为现有镜像随意替换身份。

首次安装时，将上述文件与公钥放在同一工具目录，并记录其校验清单：

```bash
cd /path/to/r3-tools
sha256sum planb-guestctl planb-backend macrn-pmp-test planb-runtime.sh \
  tvm-control tvm-driver.ko kvm.ko guest-test-client.pub > SHA256SUMS
```

在目标Host的root Bash执行本仓库host/install-host.py，明确指定工具和既有身份：

```bash
python3 host/install-host.py --tools-dir /path/to/r3-tools \
  --key /path/to/existing-guest-key --prefix /opt/sg2044-r3 \
  --entrypoint /root/zion-tests
```

安装器仅接受原生ABI1；已有入口拒绝覆盖。init会核对工具、实际/dev/tvm和历史参数接口，不能使用不匹配的Host模块。此处没有旧仓库版本升级或兼容步骤。

## 运行

在管理Host执行：

```bash
cd /root/zion-tests
./zion-runtime.sh init
./zion-runtime.sh status --json
./zion-runtime.sh select asterinas
sh ./zion-runtime.sh guest
ssh -p 10022 root@127.0.0.1
```

普通Linux使用./zion-runtime.sh linux，独立Enclave使用./zion-runtime.sh enclave；受控停止使用./zion-runtime.sh stop。独立Enclave模式无virtio-blk。镜像切换会清理Guest RAM，先保存需要的测试输出。
