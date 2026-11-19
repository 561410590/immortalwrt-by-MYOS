---
name: infocd-tool
description: 查询 MYOS 路由器的运行状态、WAN/LAN、在线终端、Wi-Fi 或蜂窝信号。
---

# 查询路由器状态

用 shell 工具调用 `myos-device-status`。命令不需要参数，返回 JSON。

- `global` 包含机型、系统运行时间、CPU、内存和当前网络类型。运行时间以秒计。
- `wans` 和 `lans` 包含已读到的接口状态。用 `up` 判断接口是否已连接，`upload`、`download` 为已取得的累计字节数。
- `terminal_all.count` 是在线终端数，`terminal_all.client` 列出在线终端。`wlan.radio` 给出无线电状态、频段和信道。
- `cpe` 给出已读到的蜂窝制式、频段和信号值。未返回此项时，说明蜂窝状态不可用。

按用户所问选取字段。缺失字段表示当前无数据。
