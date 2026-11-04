# immortalwrt by MYOS — N5000

本分支是 immortalwrt-mt798x-6.6-n5000 的 2026-11-04 公开源码快照，源码直接位于仓库根目录。

| 分支 | 机型 |
| --- | --- |
| [main](https://github.com/561410590/immortalwrt-by-MYOS/tree/main) | C8-688、C5800-688、NBCPE-688 |
| [C2000MAX](https://github.com/561410590/immortalwrt-by-MYOS/tree/C2000MAX) | C2000MAX |
| [C2000-Ultra](https://github.com/561410590/immortalwrt-by-MYOS/tree/C2000-Ultra) | C2000-Ultra |
| [N5000](https://github.com/561410590/immortalwrt-by-MYOS/tree/N5000) | N5000 |

## 获取对应源码

```sh
git clone --depth 1 --single-branch --branch N5000 https://github.com/561410590/immortalwrt-by-MYOS.git immortalwrt-mt798x-6.6-n5000
cd immortalwrt-mt798x-6.6-n5000
```

`--depth 1` 避免下载旧合并仓库的全部历史。各分支包含自己的 .config、本地修改的 feeds 和安装链接。
Ultra、N5000 原先使用的 MAX/package 已作为冻结的公开输入保存在本分支 feeds/base，不需要另克隆 MAX 分支。
主树和 MAX 的 feeds/base 使用本树 package。

## 构建输入与边界

请使用 Linux 环境。版本和当前配置目标见 SOURCE_SNAPSHOT.json，原厂运行时、参考镜像和密文凭据等外部输入见 BINARY_INPUTS.json。
factory-openwrt 按清单放在源码目录的同级。厂商二进制仍是外部依赖，不宣称均可由源码重建。
保留已附带的 feeds；需要重建索引时使用 `./scripts/feeds update -i -a`，普通 feeds 更新会覆盖这些本地修改。
本次只更新源码，没有运行固件编译或刷机验证；既有构建配置的版本号保持原值。

官方 APP 的发现、会话、MQTT、远程网页与 SSH 联动源码保留；作者私有统计／固件更新服务、真实设备身份、密码及原签名私钥不提供。
公开副本相应调整了私有服务的安装、启动和装配引用。源码中的公开支持页面链接不属于私有上报服务。
原有 COPYING、LICENSES 和各组件许可保持有效。

## N5000

保存当前 N5000 独立源码、nradio_n5000 配置、设备树及装配输入，包括 EHT Wi-Fi、网口灯、HNAT、88.1 默认地址及已落地的权限安装规则。
原 build-n5000.sh 针对作者本地缓存中的 Ultra 绝对路径使用 mount namespace；该脚本保留用于说明原环境，不是任意目录的通用构建入口。
本次没有验证干净构建。N5000 快照本身没有独立 Git 元数据，不能把继承基线提交冒称为本地全部改动已提交。
