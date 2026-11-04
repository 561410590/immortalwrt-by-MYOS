MYOS N5000 V3.1.36-202610241000：回 MYOS 恢复工具

适用于官方 2.3.5.n0.c1 创建的新 SD 分区：
p1 为可选官方存储，p2 为 1 MiB 环境预留区，
p3 为 128 MiB kernel，p4 为余下空间的 rootfs。
MYOS 可写区使用 p4 中 SquashFS 后按 64 KiB 对齐的空间。
整卡迁移到更大 SD 后，首启扩展末尾 p4 及其可写文件系统。
p1-p3、p4 起点和分区 UUID 保留；主动预留尾部空间时跳过扩容。

新布局回 MYOS：
在官方存储管理选择“第二系统优先”，保留 SD 卡后重启。
也可在官方 Ultra 上解压本归档并执行：
  sh return-myos.sh /dev/mmcblk0
脚本设置 SPI 环境 boot_from_sd=1 并回读确认。
myos-n5000-ubenv 取自本机官方系统，供 ARM64 路由器使用。
新布局的系统选择须在本机操作。

以下仅适用于旧 MYOS p1-p6 加 p7 布局：
工具校验 Ultra 专用 GPT GUID 和原分区边界。
回官方将 SD p2 名称 u-boot-env 改为 myos-stock-env；
return-myos.sh 恢复该名称，保留分区位置、UUID 和内容。
恢复工具还保存在 SD p7 的 myos-recovery 目录。
Linux 电脑也可使用分区工具将已核实的 SD 第 2 分区
名称恢复为 u-boot-env，再安全移除并插回路由器。

新版成品为 sysupgrade BIN，供官方“烧录第二系统”
及 MYOS“备份与升级”使用。实际启动、扩容和系统切换
需要在本版本 Ultra 固件上分别验证。

