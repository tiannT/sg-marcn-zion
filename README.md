# SG2044 TEE运行环境

在无H扩展的SG2044服务器上运行Asterinas、Linux机密虚拟机和独立Enclave，提供Guest启动、切换、SSH访问及功能测试工具。

## 内核文件

| 组件 | 获取与使用 |
| --- | --- |
| Asterinas | [预编译内核](prebuilt/asterinas/asterinas_kernel.bin)，按[使用说明](prebuilt/asterinas/README.md)校验并搭配对应initrd使用 |
| Linux CVM | Linux 6.6.88+的`guest_kernel_image`，校验信息见[kernel-reference.json](kernel-reference.json) |

`asterinas_kernel.bin`是RISC-V Image，可直接作为启动载荷的内核输入，无需重新编译或从ELF转换。完整启动SD镜像可写入SD卡；单个内核文件需要先放入统一启动载荷`SG2044.fd`。

## 启动Guest

在已完成[部署](docs/deployment.md)的SG2044管理Linux上，以root执行：

```bash
cd /root/zion-tests
./zion-runtime.sh init
./zion-runtime.sh select asterinas
sh ./zion-runtime.sh guest
ssh -p 10022 root@127.0.0.1
```

| Host命令 | 功能 |
| --- | --- |
| `./zion-runtime.sh status --json` | 查看Guest状态 |
| `./zion-runtime.sh linux` | 启动Linux CVM |
| `./zion-runtime.sh enclave` | 启动无块设备的独立Enclave环境 |
| `./zion-runtime.sh stop` | 停止Guest并清理私有内存 |

切换Guest前请保存需要保留的数据。

## 文档与目录

- [部署与运行](docs/deployment.md)：环境准备、Host安装和内核使用。
- [功能测试](docs/testing.md)：启动、数据库、内存保护和Enclave测试。
- [组件与许可证](THIRD_PARTY.md)：源码来源与许可信息。
- [运行环境信息](runtime-reference.json)：运行接口及组件清单。

| 路径 | 内容 |
| --- | --- |
| `prebuilt/asterinas/` | Asterinas内核、校验文件和使用说明 |
| `host/` | Host管理命令、SSH代理和安装程序 |
| `linux/` | Guest生命周期与设备后端管理 |
| `guest/`、`run-case.sh` | 功能测试脚本 |
| `source/macrn/` | 安全监控器、共享I/O和控制工具源码 |
| `source/host-driver/` | Host驱动与控制接口源码 |
| `source/linux-cvm-kernel/` | Linux CVM内核源码引用 |
