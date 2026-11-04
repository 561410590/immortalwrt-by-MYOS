require("luci.util")

mp = Map("uuplugin")
mp.title = translate("网易UU游戏加速器")
mp.description = translate("使用网易官方核心。可选择 OpenWrt aarch64 或 H3C NX30 Pro 平台；开启后在 UU 主机加速 APP 中绑定设备。")


mp:section(SimpleSection).template  = "uuplugin/uuplugin_status"

s = mp:section(TypedSection, "uuplugin")
s.anonymous = true
s.addremove = false

o = s:option(Flag, "enabled", translate("启用"))
o.default = 0
o.optional = false

o = s:option(ListValue, "platform", translate("核心平台"))
o:value("h3c-nx30pro", translate("H3C NX30 Pro"))
o:value("openwrt-aarch64", translate("OpenWrt aarch64（官方通用）"))
o.default = "openwrt-aarch64"
o.rmempty = false
o.description = translate("两个选项均只下载网易官方核心，不修改核心二进制。H3C 模式使用官方 H3C 主程序，并从官方 OpenWrt 包中取得已校验的 xtables 兼容组件；通用模式完整使用官方 OpenWrt aarch64 包。切换核心时停止旧进程并更换官方核心。")

function mp.on_after_commit(self)
	luci.sys.call("/etc/init.d/uuplugin restart >/dev/null 2>&1")
end

mp:section(SimpleSection).template  = "uuplugin/uuplugin_qcode"

return mp
