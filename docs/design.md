# SG2044 r3运行设计

SG2044无RISC-V H扩展。VM0管理Linux与VM1保护域分配独立物理Hart，由统一M态监控器实施PMP、私有现场管理、共享通道校验、I/O边界及受控Guest生命周期。

VM1的运行模式为：image1 Asterinas、image2带virtio-blk的Linux CVM、image3独立无virtio-blk的Linux Enclave。停止、擦除4GiB私有内存、重载和启动由monitor与Host控制器串行组织；VM0常驻。不会同时运行多个VM1镜像。

Host通过原生ABI1查询受保护物理地址、故障计数和一次性篡改状态。tvm兼容模块的实际越权读由PMP拒绝，Linux异常表恢复；kvm兼容模块只提供历史参数，未将无H平台表述为实现硬件KVM。

Enclave复用Zion驱动、SDK/runtime/loader及程序，在Linux CVM内部完成生命周期和跨OCALL连续性测试。M态保留私有保存状态，S3检测只作用于允许的共享字段；demo自身的LIMIT必须保留。

后端为Asterinas和普通Linux提供网络与块设备，独立Enclave模式不打开磁盘且不生成块设备节点。SSH入口绑定Host回环127.0.0.1:10022，Guest地址为10.0.2.15，不依赖工作站转发身份。

镜像内置参考软件与依赖，启动只校验并运行；不在Guest启动时展开数据库应用包。参考Linux二进制、可选Linux源码示例及公开构建输入的边界见[r3源码说明](r3-source.md)。具体判据与编号见[测试说明](testing.md)。
