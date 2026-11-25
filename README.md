# SG2044 R3运行代码

本仓库以`SG2044_原命令三项补齐_r3_20261124`交付版本为准，保留R3的运行、安装、测试代码和监控器/驱动源文件。旧仓库的构建流程、构建说明、配方及无关Guest源码已移除。

Linux CVM直接复用天权星的 **guest_kernel_image（6.6.88+）** 及配套Guest模块、用户态。R3的SG2044.fd已包含该内核，正常运行不需要重新编译内核或重新组装载荷。版本和校验值见[kernel-reference.json](kernel-reference.json)。

| 路径 | 内容 |
|---|---|
| `host/` | R3 Host命令、回环SSH代理及安装程序 |
| `linux/planb-runtime.sh` | 受控Guest停止、清理、启动与后端管理 |
| `guest/` | 原Redis、MySQL测例脚本 |
| `run-case.sh` | 按旧0001—0010编号执行单项测试并保存stdout、stderr和返回码 |
| `source/macrn/` | R3监控器、共享I/O及控制工具的C/头/汇编/链接布局源文件 |
| `source/host-driver/` | 实际PMP访问驱动、历史参数接口及tvm-control源码 |
| `source/linux-cvm-kernel/` | 固定引用Zion Guest 6.6.88源码，供核对，不参与当前部署流程 |
| `tools/reference-kernel.py` | 校验现成guest_kernel_image，或从R3载荷提取相同内核字节 |

R3保留Asterinas、带块设备Linux和独立无块设备Enclave三种模式，Host原生ABI为1。tvm兼容驱动执行真实受保护访问；kvm模块仅提供历史参数接口，不实现物理H扩展或/dev/kvm。

- [R3部署与运行](docs/deployment.md)
- [测例与证据要求](docs/testing.md)
- [组件来源](THIRD_PARTY.md)

仓库不保存运行凭据、现场日志、启动卡镜像或大型固件。使用R3交付包中的配对固件、Host二进制和既有Guest身份；源码引用不代表曾逐字重现原参考内核。
