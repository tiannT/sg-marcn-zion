# Asterinas预编译内核

`asterinas_kernel.bin`可直接作为SG2044统一启动载荷的内核输入，无需重新编译或从ELF转换。

| 项目 | 值 |
| --- | --- |
| 格式 | 带64字节启动头的未压缩RISC-V Image |
| 文件大小 | 3732764字节 |
| 运行接口 | SG2044原生ABI 1 |
| 配套initrd | `asterinas_initrd.cpio.gz`，校验值见[manifest.json](manifest.json) |
| 源码提交 | [fc16273b04ec3df80bc425ede2e3e8f768a29d13](https://github.com/sdww0/asterinas/tree/fc16273b04ec3df80bc425ede2e3e8f768a29d13) |

## 校验

在本目录执行：

```bash
sha256sum -c SHA256SUMS
```

## 使用

将内核与匹配的initrd、命令行装入`SG2044.fd`后，由安全监控器加载。完整启动SD镜像可直接烧录；此文件是单个内核，不是整卡镜像。

该内核已在SG2044硬件上验证Asterinas启动、SSH、SQLite、Redis和MySQL。其他功能的使用与测试见[部署说明](../../docs/deployment.md)和[测试说明](../../docs/testing.md)。

完整校验及来源信息见[manifest.json](manifest.json)，许可条款见[LICENSE-MPL](LICENSE-MPL)。
