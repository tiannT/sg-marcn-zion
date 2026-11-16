# SG2044 方案 B：m-ACRN、Rust OS 与 Zion 融合源码

本项目组合 m-ACRN、Asterinas 和 Zion 相关组件，在无 H 扩展的 SG2044 上提供受保护 Guest、受控 I/O 和 Linux CVM 内嵌 Enclave 的运行环境。

SG2044 不具备 RISC-V H 扩展。本方案以统一 M-mode 监控器、物理 Hart 分域和真实 PMP 为基础：VM0 管理 Linux 持续运行，VM1 在 Asterinas 与 Linux CVM 之间受控切换。Zion 安全语义适配和 Linux CVM 内的驱动、SDK、runtime 提供共享通道检查及内嵌 Enclave 功能；VM0 命令兼容层保留原 Zion 操作入口。

## 目录

| 路径 | 内容 |
|---|---|
| `src/macrn/` | 当前融合版监控器、SG2044 支持、PMP、I/O、生命周期、Enclave 和工具源码 |
| `src/asterinas/` | 实际移植后的 Asterinas、OSTD、OSDK 和源码统计脚本 |
| `src/zion-enclave/` | 当前复用的 Zion Linux 驱动、SDK、runtime、loader 和应用源码 |
| `src/linux-cvm-kernel/` | Linux 6.12.35 Git 子模块，固定上游提交，主仓库不附带整套内核源码 |
| `runtime/zion-compat/` | Zion 原命令兼容层、安装程序和当前测试脚本 |
| `runtime/guest/` | Asterinas、Linux Guest 初始化与服务脚本 |
| `configs/` | Linux CVM 构建配置与部署参数示例 |
| `recipes/` | 固定 Nixpkgs 版本的 tee-crypto、tee-vault 构建配方 |
| `scripts/` | 监控器、工具、Linux 内核构建和统一镜像组装入口 |
| `docs/` | 当前设计、测试和构建说明 |
| `UPSTREAM_VERSIONS.json` | 上游版本、固定提交与适配范围 |
| `SOURCE_MANIFEST.json` | 主仓库文件清单、SHA256 和子模块提交引用 |

## 安装到测试的使用说明

首次使用从 **[部署指南](docs/getting-started.md)** 开始，依次准备环境、Guest 身份与镜像输入，在目标 Host 安装工具和兼容层，再执行功能测试。

路径可通过安装参数配置，默认值见 [部署配置示例](configs/deployment.env.example)。目标地址由使用者指定，不依赖开发者的机器、用户名或目录。

## 下载上游依赖

Linux 使用标准 Git 子模块，GitHub 页面会显示 `src/linux-cvm-kernel @ 783cd2c`，可跳转到上游固定版本。下载本项目后，在项目根目录初始化：

```bash
bash scripts/init-submodules.sh
```

固定版本为 [Linux v6.12.35 / 783cd2c3dca8](https://github.com/gregkh/linux/tree/783cd2c3dca8b6c434e955b84c20c8940588dc68)。内核配置单独保留在 `configs/linux-cvm.config`。

## 快速检查

在源码目录执行：

```bash
python3 scripts/check-source.py
PYTHONDONTWRITEBYTECODE=1 python3 -B runtime/zion-compat/test_compat.py
PYTHONDONTWRITEBYTECODE=1 python3 -B runtime/zion-compat/test_install.py
```

这些检查不连接服务器、不切换 Guest、不触发篡改注入。

## 构建入口

先准备 Linux 构建环境和 RISC-V 交叉工具链，再执行：

```bash
export CROSS_COMPILE=riscv64-linux-gnu-
bash scripts/init-submodules.sh
bash scripts/build-monitor.sh
bash scripts/build-tools.sh
bash scripts/build-linux-cvm.sh
```

输出位于 `build/`。Asterinas、Enclave、应用和 initramfs 依赖各自工具链及用户态依赖，完整说明见 [构建说明](docs/build.md)。源码目录不包含已编译固件、OS 镜像、工具链安装包或实验凭据；组装新固件时需另外准备 VM0 载荷、板级 DTB、两种 Guest 内核及 initramfs。

## 设计与测试

- [当前设计与十项指标评估](docs/design.md)
- [安装、部署与测试](docs/getting-started.md)
- [Zion 原命令测试手册](docs/testing.md)
- [构建与外部输入](docs/build.md)
- [代码来源与适配范围](docs/code-origin.md)
- [组件来源与许可证](THIRD_PARTY.md)

测试在管理 Linux 本机执行。H 扩展指标 0004 需要 H 平台；本项目的无 H 执行机制不能替代该项硬件验证。

## 上传内容范围

仓库收录源码、配置、脚本、必要资源及使用文档。构建输出、运行状态、日志、私钥和随机种子应保存在本地目录，不加入 Git。

各组件保留其原有许可证和版权声明，详见 `THIRD_PARTY.md`。
