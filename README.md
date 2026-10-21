# immortalwrt by MYOS

MYOS 的 ImmortalWrt 源码发布仓库。三棵源码树统一放在这里，分别保存设备适配、驱动补丁、自定义软件包、本地修改的 feeds 和原始构建配置。

| 源码目录 | 主要机型 | 构建说明 |
| --- | --- | --- |
| [immortalwrt-mt798x-6.6](immortalwrt-mt798x-6.6/) | C8-688、C5800-688、NBCPE-688 | [说明](immortalwrt-mt798x-6.6/README-OPEN-SOURCE.md) |
| [immortalwrt-mt798x-6.6-c2000max](immortalwrt-mt798x-6.6-c2000max/) | C2000MAX | [说明](immortalwrt-mt798x-6.6-c2000max/README-OPEN-SOURCE.md) |
| [immortalwrt-mt798x-6.6-c2000ultra](immortalwrt-mt798x-6.6-c2000ultra/) | C2000-Ultra | [说明](immortalwrt-mt798x-6.6-c2000ultra/README-OPEN-SOURCE.md) |

## 获取与使用

```sh
git clone https://github.com/561410590/immortalwrt-by-MYOS.git
cd immortalwrt-by-MYOS/immortalwrt-mt798x-6.6-c2000max
```

进入对应源码目录，再按照其中的 `README-OPEN-SOURCE.md` 准备环境、外部输入和构建。三棵树保持同级，Ultra 的 `feeds/base` 继续使用同级 MAX 树的 package；不要任意改变目录名或只下载 Ultra。

原厂运行时和参考镜像等外部资产按各树的 `BINARY_INPUTS.json` 自行准备，`factory-openwrt/` 应放在本仓库根目录内，与三棵源码树同级。本地修改后的 feeds 已随源码保存，请使用各树的 `scripts/prepare-source.sh` 重建索引，避免普通 feeds 更新覆盖这些修改。

## 公开范围

官方 APP 的发现、会话、MQTT、远程网页和 SSH 联动源码保留。连接、上报到作者自有域名服务器的私有服务及其客户端不在公开范围；真实设备身份、密码和原签名私钥也不包含。APP 联动所需的原厂密文凭据由构建者自行提供。

本仓库保存目前可提供的源码和适配，部分厂商组件仍依赖二进制输入。公开副本尚未完成所有机型的全量干净重编和刷机验收，本次不发布新固件。

## 许可证

原有版权和许可证声明保持原样，各组件适用各自许可证。请查阅各源码树的 `COPYING`、`LICENSES/` 和组件内的许可文件；仓库名称及 MYOS 标识不改变第三方组件的权利归属。
