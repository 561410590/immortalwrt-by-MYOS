MYOS C2000MAX V3.1.38-202610301800：回 MYOS 恢复工具

新版官方 SD 布局：p1 可选官方扩展存储，p2 环境预留区，
p3 内核，p4 根分区。容量按实际卡与用户分配识别。
整卡复制到更大 SD 时，首启仅扩展末尾 p4 及其可写文件系统。
p1-p3、p4 起点和各分区 UUID 保留；预留尾部空白的自定义布局跳过扩容。
在官方存储管理选择“第二系统优先”，保留 SD 卡后重启。
命令行替代方式：解压本归档，在官方 MAX 上执行
  sh return-myos.sh /dev/mmcblk0
脚本设置 SPI 环境 boot_from_sd=1 并回读确认，保留 GPT 和数据。
myos-max-ubenv 为本机官方 ARM64 musl 工具；新版 SPI 启动设置
须在 MAX 本机操作，PC 改分区名无效。

以下仅适用于旧 MYOS p1-p6 加 p7 的布局：
旧布局校验 MAX 指定 GPT GUID 与 p1-p6 边界。
回官方切换将 SD p2 分区名 u-boot-env 改为 myos-stock-env；
恢复时还原该名称，保留各分区位置、UUID与内容。

方法一：官方系统 / TTL 控制台
解压归档，进入工具目录，执行：
  sh return-myos.sh /dev/mmcblk0
成功后重启路由器，保留 SD 卡。

工具也保存在 SD p7 的 myos-recovery 目录；
可在官方系统挂载 SD p7 后从该目录执行。

方法二：Linux 电脑
插入 SD 卡，准确识别对应磁盘。
使用 parted 将该卡第2分区名称还原为 u-boot-env，
例如针对已核实的 /dev/sdX：
  sudo parted -s /dev/sdX name 2 u-boot-env
安全移除后插回 MAX，重新开机。
归档中的可执行文件是 ARM64 静态程序，用于路由器。

一键回官方依据用户提供的 Ultra TTL 回退链设计。
本版启动、扩容和系统切换均以实际刷入结果为准。
仅旧布局通过调整 SD GPT 选择启动系统。
