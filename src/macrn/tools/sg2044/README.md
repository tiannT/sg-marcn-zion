# SG2044 构建与运行工具

本目录提供板级构建、统一载荷组装、Host 控制与 I/O 后端、PMP 测试及 Enclave 探针源码。

从仓库根目录使用公共构建入口：

```bash
bash scripts/build-monitor.sh
bash scripts/build-tools.sh
python3 scripts/package-firmware.py --help
```

- [首次部署](../../../../docs/getting-started.md)：环境、凭据、构建、加载和启动。
- [构建说明](../../../../docs/build.md)：组件与外部输入。
- [测试手册](../../../../docs/testing.md)：逐项命令、预期输出和判据。

运行控制和设备后端在目标管理 Linux 中执行。镜像、板级凭据、运行状态和日志在本地准备，不属于源码发布内容。
