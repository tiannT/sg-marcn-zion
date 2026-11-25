# 构建说明

本目录提供当前源代码和源码构建入口。工具链、基础 OS 载荷、用户态软件闭包、SSH 凭据和板卡启动文件分别准备，不随源码上传。

## 1. 监控器与运行工具

准备 Make、Python 3、RISC-V GCC/binutils 及静态 libc 开发文件。`CROSS_COMPILE` 指定交叉编译器前缀，也可包含安装目录。

```bash
export CROSS_COMPILE=riscv64-linux-gnu-
bash scripts/build-monitor.sh
bash scripts/build-tools.sh
```

监控器配置为 SG2044、两个 Hart、Linux 静态域入口，启用 `LIVE_PMP_TEST`、`PLAN_B_SERVICES`、`PLAN_B_ENCLAVE`、`PLAN_B_RUNTIME`。构建入口在 `src/macrn/tools/sg2044/build.py`，包含镜像布局检查和 H 指令扫描。输出位于 `build/monitor/`。

运行工具输出到 `build/tools/`，包括 `planb-guestctl`、`planb-backend`、`macrn-pmp-test`、`tvm-control` 和 `planb-runtime.sh`。部署时同时使用本次构建的固件和工具，并保留对应的版本、配置及哈希。

## 2. 可选Linux源码构建示例

r3参考内核是外部Linux6.6.88+及其匹配模块，见[r3源码说明](r3-source.md)。以下Linux6.12.35只作为可选源码构建示例，不代表重现r3参考二进制。

Linux 作为 Git 子模块固定到 `v6.12.35` 对应提交 `783cd2c3dca8b6c434e955b84c20c8940588dc68`。主仓库只保存引用和配置。首次构建先下载：

```bash
bash scripts/init-submodules.sh
bash scripts/build-linux-cvm.sh
```

入口使用随包保存的 `configs/linux-cvm.config`，在 `build/linux-cvm/` 进行独立输出构建。内核为 `build/linux-cvm/arch/riscv/boot/Image`；Guest 内核模块应针对该次内核的构建目录编译。

获取固定子模块需要网络；初始化完成后可以使用本地工作树构建。项目通过子模块引用上游内核，定制配置保存在主仓库。

Host模块源码位于`runtime/host-driver/`。设置`KERNEL_BUILD_DIR`为目标管理Linux完全匹配的配置/生成头/Module.symvers构建树，再运行`bash scripts/build-host-driver.sh`。输出`tvm-driver.ko`和历史参数兼容`kvm.ko`到build/tools；后者不实现H虚拟化。

## 3. Asterinas

完整源码、OSDK、依赖锁定文件和工具链要求均在 `src/asterinas/`。当前移植基于 `UPSTREAM_VERSIONS.json` 中的提交；实际修改已保留在源文件中。

在该组件要求的开发容器和 Rust 工具链环境内执行其 `Makefile`/OSDK 构建流程，目标架构为 `riscv64`。开发容器镜像为 `asterinas/asterinas:0.17.2-20260508`；Rust 版本以该源码的 `rust-toolchain.toml` 为准。源码目录没有附带 Cargo/rustup 缓存、OSDK 可执行文件或容器镜像。

在包含 Rust、Nix 和组件依赖的构建环境中，从仓库根目录执行：

```bash
make -C src/asterinas TARGET_ARCH=riscv64 RELEASE=1 \
  SCHEME=riscv SMP=1 CONSOLE=ttyS0 kernel
```

该命令会构建 OSDK、组件基础 initramfs 和内核，不会启动 QEMU。Nix/Cargo 首次获取依赖需要网络。用户需再按 [Guest rootfs 准备](guest-rootfs.md)加入本项目的初始化与测试服务，不能直接把通用 initramfs 当作测试镜像。

VM1 使用未压缩的 RISC-V Image。将 OSDK 构建日志所指向的内核 ELF 转换为原始镜像，输入的 ELF 必须来自上述构建：

```bash
read -r -p 'Asterinas kernel ELF path: ' ASTERINAS_ELF
mkdir -p inputs/asterinas
"${CROSS_COMPILE:-riscv64-linux-gnu-}objcopy" -O binary \
  "$ASTERINAS_ELF" inputs/asterinas/Image
```

内核 Image、定制 initramfs 和单行启动参数文件分别交给统一镜像组装器。

## 4. Enclave、应用和用户态依赖

`src/zion-enclave/` 包含 Linux Guest 驱动、SDK、Eyrie runtime、loader 和示例程序源码。需要 CMake、RISC-V GCC/G++、静态 libc 与 libstdc++。先完成 Linux 内核构建，再从仓库根目录构建组件：

```bash
export PROJECT_DIR="$PWD"
export CROSS_COMPILE=riscv64-linux-gnu-
export ZION_SDK_DIR="$PROJECT_DIR/build/zion/sdk-install"
mkdir -p "$PROJECT_DIR/build/zion/runtime-output"
cmake -S src/zion-enclave/sdk -B build/zion/sdk \
  -DZION_SDK_DIR="$ZION_SDK_DIR" -DZION_BITS=64 \
  -DCROSS_COMPILE="$CROSS_COMPILE" -DCMAKE_BUILD_TYPE=Release
cmake --build build/zion/sdk -j2
cmake --install build/zion/sdk
cmake -S src/zion-enclave/runtime -B build/zion/runtime \
  -DZION_SDK_DIR="$ZION_SDK_DIR" \
  -DCMAKE_SYSTEM_NAME=Generic \
  -DCMAKE_C_COMPILER="${CROSS_COMPILE}gcc" \
  -DCMAKE_ASM_COMPILER="${CROSS_COMPILE}gcc" \
  -DCMAKE_OBJCOPY="${CROSS_COMPILE}objcopy" \
  -DEYRIE_OUTPUT_DIR="$PROJECT_DIR/build/zion/runtime-output"
cmake --build build/zion/runtime -j2
make -C src/zion-enclave/examples demo \
  CROSS_COMPILE="$CROSS_COMPILE" SDK_DIR="$ZION_SDK_DIR" \
  BUILD_DIR="$PROJECT_DIR/build/zion/examples"
make -C build/linux-cvm ARCH=riscv CROSS_COMPILE="$CROSS_COMPILE" \
  M="$PROJECT_DIR/src/zion-enclave/driver/kernel" \
  ZION_SDK_DIR="$ZION_SDK_DIR" modules
"${CROSS_COMPILE}gcc" -O2 -Wall -Wextra -static \
  src/zion-enclave/test/zion-driver-security.c \
  -o build/zion/zion-driver-security
```

SDK 安装在 `build/zion/sdk-install/`；runtime 与 loader 在 `build/zion/runtime-output/`；demo 在 `build/zion/examples/`。内核模块生成于 `src/zion-enclave/driver/kernel/`，属于被忽略的本地构建输出。驱动必须与本次 Linux CVM 内核匹配，Guest 内以 `nested_cvm=1` 加载。

将这些输出按 [Guest rootfs 准备](guest-rootfs.md)放入 Linux Guest。该集成使用本仓库的 M-mode 实现，组件上游文档中完整 Zion 工程的 `utils/`、OpenSBI 和 QEMU 构建路径不适用于本仓库。

补充的隔离探针与上下文审计源码位于 `src/macrn/tools/sg2044/` 和 `src/macrn/tests/riscv/`；需要执行测试手册的补充检查时，需额外构建并装入 Guest。

`recipes/tee-apps.nix`保留旧的独立应用示例，以下可选命令不属于r3参考镜像的准备步骤；r3数据库用户态按[参考initramfs转换](guest-rootfs.md)准备：

```bash
mkdir -p build
nix-build recipes/tee-apps.nix -o build/tee-apps
```

initramfs 还需准备 BusyBox、SSH 服务、动态库闭包、SQLite speedtest1，以及对应 Guest 初始化脚本。初始化脚本在 `runtime/guest/`；Nix 构建程序的解释器和依赖闭包须完整保留。SSH 私钥和 Host key 由本地独立生成，不加入源码仓库。

完整文件布局和打包命令见 [Guest rootfs 准备](guest-rootfs.md)。外部用户态软件须与所选 OS 的 ABI 和目标架构匹配。

## 5. 组装一份统一固件

准备以下输入，放在本地 `inputs/` 等忽略目录中：

| 输入 | 说明 |
|---|---|
| VM0 Image、initramfs、单行启动参数文件 | 与目标 SG2044 管理系统匹配的现有载荷 |
| Asterinas Image、initramfs、启动参数文件 | 使用r3参考转换器生成的 `/init-planb-asterinas` 入口 |
| Linux CVM Image、initramfs、启动参数文件 | 含 SSH、virtio 和需要的 Enclave 组件 |
| 板级 DTB | 使用目标平台的完整有效设备树，含 `/chosen` |
| `fw_dynamic.bin` | 本次监控器构建输出 |

查看参数：

```bash
python3 scripts/package-firmware.py --help
```

SG2044板级DTB必须使用r3原生rv64imafdcv描述；组装器清除残留扩展列表并保留启动FDT槽，执行254MiB加载边界检查。

为三个域配置提供 `--vm0-*`、`--asterinas-*`、`--linux-*` 参数，并指定 `--board-dtb`、`--firmware`、`--output-dir`。输出为一对 `fw_dynamic.bin` 和 `SG2044.fd`；VM0 常驻，VM1 可切换。

组装入口为每个新输出注入独立的单次启动实验随机种子，禁止复用已有输出目录，不打印种子内容。该机制不构成生产级熵源或认证启动链。输出是包含私有实验输入的本地构建产物，位于 `dist/` 等忽略目录，不随源码上传。

烧录、Host 重启与板级验收由操作人员安排，本目录中的构建入口不会自动部署固件或操作启动卡。

## 6. 命令兼容层

`runtime/zion-compat/` 保存 `zion-runtime.sh`、Python 适配程序、安装程序和测试脚本。安装依赖目标 Host 上的方案 B 工具、匹配的固件 ABI 及本地 Guest 凭据。首次部署的完整顺序及参数见[首次部署指南](getting-started.md)，逐项检查见[测试手册](testing.md)。

安装程序会保留原文件备份；运行时不引入版本回退框架。源码仓库不包含这些备份或任何服务器状态。

部署工具目录还需本地 `guest-test-client.pub` 和 `SHA256SUMS`：公钥由本地 Guest 测试私钥导出，并应与 Guest 内授权公钥匹配；校验清单覆盖七个运行文件（四个工具、一个脚本、两个Host模块）及该公钥。不能以重新计算哈希替代工具来源和固件 ABI 的核对。私钥始终放在源码目录之外。首次安装的完整命令见[安装、部署与测试说明](getting-started.md)。

安装路径由 `--prefix`、`--tools-dir`、`--key`、`--entrypoint` 和 `--backup-dir` 指定，默认值见 `configs/deployment.env.example`。安装程序检查端口与真实控制接口，备份已有工具及 SSH 客户端配置，再安装兼容入口；不会修改 SSH 服务端配置。
