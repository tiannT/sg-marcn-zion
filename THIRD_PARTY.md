# 组件来源

监控器和控制工具源文件来自R3交付包所记录的m-ACRN基础源码及已应用的四份适配补丁，保留原文件版权/SPDX和[source/macrn/LICENSE](source/macrn/LICENSE)。Host驱动源文件直接取自同一R3交付包。

Linux CVM的源码引用为sdww0/rockos-kernel的zion-guest固定提交ba3a4296ab14d57143c8398cfe493696b61207c5，其基础版本为6.6.88。实际运行复用既有6.6.88+参考Image及匹配模块；两者的二进制来源标识见kernel-reference.json，不将源码引用声称为参考Image的逐字复现证明。

Guest内的Asterinas、BusyBox、SQLite、Redis、MySQL和Zion Enclave程序继续使用R3交付的配套版本，各自保留其原有许可证与版权。未将旧仓库中的另一套Guest源码或内核版本作为R3运行输入。
