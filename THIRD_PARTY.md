# 组件来源与许可证

本源码目录由多个组件组成，各文件沿用原有许可证与版权声明。本清单不将不同许可证合并为一种许可证。

| 组件 | 来源 | 许可证入口 |
|---|---|---|
| m-ACRN 及 SG2044/方案 B 适配 | 上游 `intel/acrn-riscv` 基础上的当前融合工作树 | `src/macrn/LICENSE` 及文件 SPDX 声明 |
| Asterinas | `sdww0/asterinas` 固定提交基础上的实际移植工作树 | `src/asterinas/LICENSE-MPL`、`COPYRIGHT` 及各子组件声明 |
| Zion Enclave | `sdww0/zion-env` 中的 `zion-enclave/` 当前源码 | `src/zion-enclave/LICENSE` 及各子组件声明 |
| Zion CVM 安全管理模型 | `sdww0/zion` 的 CVM、上下文及保护区管理设计，适配实现合入当前 m-ACRN | 当前适配源文件声明；上游来源记录于 `UPSTREAM_VERSIONS.json` |
| Linux CVM | `gregkh/linux` 的 Linux 6.12.35 固定提交，以 Git 子模块引用 | 初始化后见 `src/linux-cvm-kernel/COPYING`、`LICENSES/` 及文件 SPDX 声明 |

具体上游提交、版本和本地修改范围见 `UPSTREAM_VERSIONS.json`。完整 Git 历史和旧版本快照未包含在本目录中。

Linux 内核和 Asterinas 均为已有开源操作系统主体；本项目的新增和适配范围见 [代码来源说明](docs/code-origin.md)。
