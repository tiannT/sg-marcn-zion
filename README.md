# SG2044：m-ACRN、Asterinas 与 Zion 集成源码

当前代码对应 **r3 原命令三项补齐版本**。SG2044 无 RISC-V H 扩展，使用 M-mode 监控器、物理 Hart 分域和 PMP：VM0 管理 Linux 常驻，VM1 可受控切换 Asterinas、Linux CVM 和独立 Enclave 模式。

r3 提供真实 `/dev/tvm` 受保护内存访问接口、`/sys/module/kvm/parameters/zion_vcpu_tamper` 一次性 S3 注入接口，以及无 virtio-blk 的独立 image3。`kvm.ko` 提供历史参数兼容，不实现 H 扩展或 `/dev/kvm`。Guest 软件在构建时直接内置 initramfs，启动过程无需解开应用压缩层。

首次使用请从[安装与部署](docs/getting-started.md)开始，再按[测试说明](docs/testing.md)执行。源码来源及公开构建入口的参数化改动见[r3版本说明](docs/r3-source.md)。

| 路径 | 内容 |
|---|---|
| `src/macrn/` | r3 M-mode 监控器、SG2044、PMP、共享I/O、生命周期与控制工具源码 |
| `runtime/host-driver/` | `tvm-driver`、历史参数模块及`tvm-control`源码 |
| `runtime/zion-compat/` | Zion命令前端、回环SSH代理、安装器及本地单元检查 |
| `runtime/planb-runtime.sh` | 原生Guest与后端生命周期控制 |
| `runtime/run-case.sh`、`runtime/guest/` | 原测例入口、数据库负载及Guest初始化脚本 |
| `src/asterinas/`、`src/zion-enclave/` | Asterinas移植源码和Zion Enclave组件源码 |
| `src/linux-cvm-kernel/` | 可选Linux 6.12.35源码构建示例的固定Git子模块 |
| `scripts/`、`configs/` | 参数化构建、参考initramfs转换、统一载荷组装及配置 |
| `SOURCE_MANIFEST.json`、`UPSTREAM_VERSIONS.json` | 源码哈希、子模块引用与组件来源 |

r3参考Linux CVM内核为 **6.6.88+**，内核与匹配的Guest模块、用户态需作为外部输入准备。仓库内可选Linux 6.12.35子模块不能重现该参考二进制；具体边界见[构建说明](docs/build.md)。

在仓库根目录检查源码和Host前端：

```bash
python3 -B scripts/check-source.py
python3 -B -m unittest discover -s runtime/zion-compat -p 'test_*.py'
```

构建r3监控器及工具：

```bash
export CROSS_COMPILE=riscv64-linux-gnu-
bash scripts/build-monitor.sh
bash scripts/build-tools.sh
```

Host模块还需与目标管理Linux完全匹配的内核构建目录，运行`bash scripts/build-host-driver.sh`前设置`KERNEL_BUILD_DIR`。统一载荷组装需准备VM0、Asterinas和Linux CVM各自的内核、initramfs、启动参数及有效板级DTB。

仓库保存源码、配置和面向首次使用者的文档。构建输出、镜像、身份文件、运行状态及实测日志保留在本地，不加入Git。各组件保留原有版权与许可证，见[组件来源](THIRD_PARTY.md)。
