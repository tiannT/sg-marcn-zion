# r3源码版本与输入边界

此版本以`SG2044_原命令三项补齐_r3_20261124`交付包为来源。监控器由包内基础源码依次应用`asterinas-switch.patch`、`direct-initramfs.patch`、`linux6688-compat.patch`和`original-cases.patch`还原；上传前与交付包1538项源码哈希逐项核对。

当前源码直接保留已应用的修改，构建无需重复打补丁。交付包的来源标识及组件引用保存在`UPSTREAM_VERSIONS.json`，本仓库逐文件哈希在`SOURCE_MANIFEST.json`。

公开入口将原交付脚本中的机器路径改为命令行参数或安装配置；Host前端从自己的安装目录加载配置。SSH直接连接的已知指纹文件按Host启动ID与Guest epoch隔离，回环入口仍按实际Guest刷新，保留原SSH身份核验。监控器及Host驱动的核心源码与r3包一致。

| 项目 | r3机制 |
|---|---|
| 内存访问保护 | `tvm-driver.ko`注册`/dev/tvm`，实际Host读触发PMP故障，Linux异常表恢复，核对monitor故障计数及驱动证据 |
| vCPU篡改检测 | `kvm.ko`暴露历史sysfs参数，后端对允许的共享S3字段进行一次注入，M态保留私有现场并告警；一次性状态由monitor持有 |
| 独立Enclave | `enclave`进入image3，FDT与后端均禁用virtio-blk；Linux模式image2仍提供块设备 |
| 控制台 | 读取monitor留存的真实Guest控制台字节；M态物理串口另行记录 |
| 内置应用 | 参考initramfs转换器保留原程序与依赖字节，直接作为CPIO条目提供，不在Guest启动时展开应用层 |

## 内核和用户态

r3参考Guest的Linux内核为6.6.88+，SHA256为`92aad425c161139362a64913b254f9d06610b5d3f37b1f4b9a9f74cd19ce90c2`。本次交付没有提供能独立确认该二进制构建的完整匹配源码链，因此本仓库将其明确列为外部参考输入，不把其他Linux提交声称为同一内核。

原有`src/linux-cvm-kernel`仍固定Linux v6.12.35提交，作为可选源码构建示例；使用它时须重新构建匹配的Guest Enclave模块并自行验证兼容性。Host管理Linux与Guest Linux是两个不同的内核；Host `tvm-driver.ko`和`kvm.ko`必须针对管理Linux的实际版本、配置和符号CRC构建。

参考initramfs中的BusyBox 1.30.1、SQLite 3.46.1、Redis 7.0.15、MySQL 8.0.46以及Enclave程序由使用者准备。转换器只替换原生初始化适配、公钥及必要PMP辅助程序，保存其余条目和程序哈希；转换本身不等于板端验收。

## 测试和部署

公开[测试说明](testing.md)提供命令与预期判据，不上传机器成绩和完整现场串口。`runtime/run-case.sh`沿用旧0001—0010编号，文档给出当前非H编号映射。保护和篡改两项分别需要现场M态原文，参数0本身不能证明一次性窗口可用。

烧录、重启和物理串口采集由操作人员安排。构建脚本不会连接目标板或写启动介质。
