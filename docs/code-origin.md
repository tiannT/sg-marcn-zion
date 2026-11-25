# 代码来源与适配范围

本项目组合已有开源组件和 SG2044/方案 B 适配代码。源码目录中的操作系统与框架不能整体标注为本项目从头编写。

| 部分 | 主体来源 | 本项目的工作 |
|---|---|---|
| m-ACRN | `intel/acrn-riscv` 上游基础 | SG2044 无 H 运行与设备适配，以及当前 PMP、共享 I/O、Guest 生命周期、Enclave 和上下文管理实现 |
| Asterinas | `sdww0/asterinas` 的固定提交基础 | RISC-V 启动、内存布局及 virtio 网络等实际移植修改，运行 Rust OS 及应用 |
| Linux CVM 内核 | 社区 Linux 6.12.35 | 定制内核配置、编译、启动与用户态运行环境对接；CVM 隔离由统一 M-mode 监控器和 PMP 提供 |
| Zion Enclave | `sdww0/zion-env` 中的驱动、SDK、Eyrie runtime、loader 和示例程序 | 复用 Guest 软件组件，在方案 B 的监控器中实现对应兼容服务；不整体搬入原 H/KVM 执行后端 |
| Zion CVM 安全管理模型 | `sdww0/zion` 中的 CVM、上下文、PMP 和共享通道管理设计 | 在当前 m-ACRN 监控器中适配实现对应安全语义；相关实现位于 `src/macrn/` |
| Zion 原命令兼容层 | 本项目新增的 Shell/Python 适配实现，承接原 Zion 操作形式 | 将同名上层命令映射至实际方案 B 工具、Guest SSH 和运行状态 |
| 构建、打包和检查入口 | 本项目脚本与各组件原构建系统 | 汇总当前配置、提供独立输出路径、组装统一载荷和核对源码完整性 |

## Linux CVM 的来源

Linux CVM 使用 Linux 6.12.35 内核主体，不能称为本项目自行编写的 Linux 内核。它也不是直接复制原 Zion 测试环境的 `Linux zion-cvm 6.6.88+` 内核。

该内核以 Git 子模块引用 [上游固定提交 783cd2c3dca8](https://github.com/gregkh/linux/tree/783cd2c3dca8b6c434e955b84c20c8940588dc68)，主仓库保存引用及 `configs/linux-cvm.config`，不附带整套内核副本或额外内核补丁。

本方案以社区内核和当前配置为基础，在 m-ACRN 的受保护 VM1 中运行。设备树投影、虚拟设备服务和域间访问限制主要位于监控器；Guest 内使用对应的 Linux 驱动。Guest 软件组件复用包括 Zion Enclave 驱动、SDK、runtime、loader 和示例；监控器还适配了 Zion 的 CVM 安全管理语义。

## Zion 本身是否包含 Linux CVM

**包含。** 原 Zion 工程有 Linux CVM 的完整运行链路，不能将 Zion 的作用概括为仅提供 Enclave。其工程划分包括：

| 原 Zion 组成 | 作用 |
|---|---|
| `zion-host` | Host Linux 及 KVM 相关适配；来源为 `sdww0/rockos-kernel` 的同名分支 |
| `zion-guest` | Linux CVM Guest 内核适配；来源为同一仓库的 `zion-guest` 分支 |
| `tvm-driver` / `tvm-control` | Host 侧受保护虚拟机的控制接口 |
| `zion/src/cvm.c`、`context.c`、`pmp.c` 等 | M-mode CVM 生命周期、私有上下文及内存保护 |
| QEMU、virtio 与共享通道相关部分 | 原后端中的 Guest 启动、设备和 Host/Guest 交互 |

方案 B 保留其 CVM 安全管理目标并适配相关语义，用 m-ACRN 的无 H 运行、控制接口和 I/O 后端承接原 H/KVM/tvm 后端的对应职责。当前选择 Linux 6.12.35，是本方案的内核与运行环境选型，不表示 Zion 没有自己的 Linux CVM 适配，也不表示全部 CVM 功能由本项目独立原创。

本源码目录保存当前实际参与构建的代码：CVM 安全语义的适配实现位于 `src/macrn/hypervisor/arch/riscv/` 的 `planb_runtime.c`、`planb_io.c`、`static_vm.c` 等文件；复用的 Guest Enclave 组件位于 `src/zion-enclave/`。原 Zion 的 Host/Guest Linux 分支和 H/KVM 后端没有整体混入当前源码目录。

版本与提交信息见 [UPSTREAM_VERSIONS.json](../UPSTREAM_VERSIONS.json)；项目文件及子模块引用由 [SOURCE_MANIFEST.json](../SOURCE_MANIFEST.json) 固定。实际适配源码和构建配置均属于版本记录的一部分。


## r3更新

当前监控器和Host驱动源自r3交付包。Host原生tvm驱动、历史kvm参数兼容和独立image3的范围见[r3源码说明](r3-source.md)；这不等于移植了物理H/KVM后端。参考Linux6.6.88+与匹配用户态是外部输入，仓库Linux6.12.35子模块为可选示例，不能混称同一内核。
