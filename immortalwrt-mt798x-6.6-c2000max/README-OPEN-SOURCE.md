# MYOS 源码快照

这是 2026-10-21 整理的三棵 ImmortalWrt 源码工作副本。包含当前可取得的源码、设备适配、驱动补丁、自定义软件包、本地修改后的 feeds、安装链接和编译配置。原有版权声明与许可证保留，组件分别适用其原许可证。

官方 APP 的发现、会话、MQTT、远程网页与 SSH 联动适配保留。作者私有统计、在线固件分发服务及其客户端不在公开范围；真实设备身份、密码、签名私钥也未包含。公开副本对这些私有部分的打包、启动和检查引用做了相应调整。

这不是所有厂商组件均可从源码生成的固件工程。现有装配流程仍使用原厂运行时、固件参考包和若干二进制组件；仓库不会把这些文件称作源码。必须先按 `BINARY_INPUTS.json` 准备对应输入。公开副本尚未完成全部机型的干净重编或刷机验收，本次也没有发布新固件。

## 仓库布局

| 目录名 | 主要机型 | 保存的配置目标 | 保存的版本值 |
| --- | --- | --- | --- |
| immortalwrt-mt798x-6.6 | C8-688、C5800-688、NBCPE-688 | nradio_c5800-688 | 3.1.35-202610161200 |
| immortalwrt-mt798x-6.6-c2000max | C2000MAX | nradio_c2000-max | V3.1.35-202610211000 |
| immortalwrt-mt798x-6.6-c2000ultra | C2000-Ultra | nradio_c2000-ultra | V3.1.35-202610180300 |

主树的 C8/NBCPE 流程复用该树的内核和软件包输入，再执行各自的装配脚本；表中的配置与版本只是原工作副本实际保存的值，不代表此公开副本已经生成同版固件。

Ultra 原工作副本的 `feeds/base` 依赖 MAX/package。为保留实际依赖而不携带服务器绝对路径，请将三个仓库同级放置：

```text
workspace/
├── immortalwrt-mt798x-6.6/
├── immortalwrt-mt798x-6.6-c2000max/
├── immortalwrt-mt798x-6.6-c2000ultra/
└── factory-openwrt/                 # 按二进制输入清单准备原厂资产
```

## 准备与编译

使用 Linux 编译环境，并先满足上游 README 中的依赖。整理时的主机使用 Python 3.11。`SOURCE_SNAPSHOT.json` 记录上游提交和 feeds 来源提交；每份 feeds 的本地修改直接包含在仓库中，不依赖移动分支恢复。

```sh
./scripts/prepare-source.sh
python3 scripts/check-source-inputs.py --model c2000-max
make defconfig
make -j4 download
make -j4
```

`--model` 可选 `c8-688`、`c5800-688`、`nbcpe-688`、`c2000-max`、`c2000-ultra`，应与当前仓库及实际构建对象对应。检查器只核对输入，不写配置或启动编译。缺少原厂资产时会列出具体路径；不能把缓存环境中的成功当作干净重编证明。

**不要用不带 `-i` 的 `scripts/feeds update -a` 覆盖本快照的 feeds。** `prepare-source.sh` 只重建已附带源码的索引，保留本地修改和现有安装选择。

上游 feeds 工具在刷新索引时也可能按依赖规范化 `.config`；仓库保存的是原工作副本的配置原件，不把这种规范化结果冒充原始配置。

主树的额外装配入口分别位于：

- C5800：`target/linux/mediatek/image/assemble-c5800.py`，由镜像规则调用。
- C8：`target/linux/mediatek/image/nradio-c8-688/assemble-c8.py` 与 `build-c8.py`。
- NBCPE：`target/linux/mediatek/image/nradio-nbcpe-688/assemble-nbcpe.py` 与 `build-nbcpe.py`。

先准备这些入口所需的内核、IPK 和原厂输入，再使用脚本的实际参数。它们沿用原项目的二次装配方式，不应仅替换 profile 就声称得到相同固件。

APP 联动所需的原厂密文凭据应由构建者自行准备。每台设备的身份从自身工厂数据初始化，不应将其他设备的身份文件放进共享固件。包签名密钥由构建者自行生成，作者原签名私钥不提供。

公开构建在离线 rootfs 装配时调用 `scripts/prepare-public-rootfs.py`，避免旧基础镜像残留被排除的私有服务。它只接受当前源码目录内已经装配的离线 rootfs，不能用于在线路由器根目录。
