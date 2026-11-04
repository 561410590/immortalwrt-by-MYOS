local sys = require "luci.sys"
local util = require "luci.util"
local uci = require "luci.model.uci".cursor()

local function normalize_mac(value)
	local mac = tostring(value or ""):gsub("[%s:%-]", ""):upper()
	if not mac:match("^%x%x%x%x%x%x%x%x%x%x%x%x$") then return nil end
	mac = mac:gsub("(%x%x)", "%1:"):sub(1, 17)
	if mac == "00:00:00:00:00:00" or mac == "FF:FF:FF:FF:FF:FF" or
	   mac == "00:66:88:00:00:00" then return nil end
	return mac
end

local m = Map("myos_nros", translate("官方 APP 远控设置"), translate("控制鲲鹏/NRadio 官方 APP 云端在线、信号、流量、设备查看和远程登录能力。局域网 APP 发现和管理独立保持启用。"))

local status = m:section(SimpleSection)
status.template = "myos_nros/status"

local s = m:section(NamedSection, "main", "cloud", translate("APP 远控服务"))
s.addremove = false
s.anonymous = true

local enabled = s:option(Flag, "enabled", translate("启用官方 APP 远控/在线"))
enabled.default = "1"
enabled.rmempty = false
enabled.description = translate("开启后 APP 可通过官方云端显示在线、蜂窝信号、流量、终端列表，并可远程登录 LuCI。关闭后只停止 mosquitto 与云端 agent，不影响同一局域网内 APP 发现和管理。")

local remote_web = s:option(Flag, "remote_web_enabled", translate("允许 APP 远程登录 LuCI"))
remote_web.default = "1"
remote_web.rmempty = false
remote_web:depends("enabled", "1")
remote_web.description = translate("允许官方 rweb 临时网页通道，APP 鉴权通过后建立 LuCI 登录会话。")

local remote_ssh = s:option(Flag, "remote_ssh_enabled", translate("允许 APP 远程 SSH"))
remote_ssh.default = "1"
remote_ssh.rmempty = false
remote_ssh:depends("enabled", "1")
remote_ssh.description = translate("允许官方 rssh 临时通道和客户端会话临时公钥登录；无需再次输入 root 密码。")

local current_mac = s:option(DummyValue, "_current_mac", translate("当前 APP/远控 MAC"))
function current_mac.cfgvalue(self, section)
	return uci:get("oem", "board", "id") or translate("未生成")
end

local manual_mac = s:option(Value, "manual_mac", translate("背部标签 MAC"))
manual_mac.rmempty = true
manual_mac.placeholder = "AA:BB:CC:DD:EE:FF"
manual_mac.description = translate("仅在自动识别错误或部分远控失败时填写设备背部标签 MAC。保存后 APP、本地身份、MQTT 和远程登录统一改用此值；不会修改设备码或云密码。")
function manual_mac.validate(self, value, section)
	if value == nil or tostring(value):match("^%s*$") then return "" end
	local mac = normalize_mac(value)
	if not mac then return nil, translate("请输入有效的 12 位设备标签 MAC") end
	return mac
end

function m.on_after_commit(self)
	uci:load("myos_nros")
	local mac = uci:get("myos_nros", "main", "manual_mac") or ""
	if mac ~= "" then
		sys.call("/usr/sbin/myos-nros-cloudctl set-mac " .. util.shellquote(mac) .. " >/dev/null 2>&1")
		return
	end
	if uci:get("myos_nros", "main", "enabled") == "1" then
		sys.call("/usr/sbin/myos-nros-cloudctl enable >/dev/null 2>&1")
	else
		sys.call("/usr/sbin/myos-nros-cloudctl disable >/dev/null 2>&1")
	end
end

return m
