local modem = require "myos_c8_modem"
local snapshot = require("luci.controller.modem").read_status()
local settings, sim, device = snapshot.settings or {}, snapshot.sim or {}, snapshot.modem or {}
local fresh = snapshot.cache_state == "fresh"
local labels = { ["1"] = translate("SIM 1（外置下槽）"), ["2"] = translate("SIM 2（外置上槽）"),
    ["3"] = translate("内置联通"), ["4"] = translate("内置移动") }
local physical = fresh and labels[sim.slot] and sim.slot or nil
if not fresh then
	local uci = require("luci.model.uci").cursor()
	local saved_slot = uci:get("cpesel", "sim", "cur")
	saved_slot = labels[saved_slot] and saved_slot or "1"
	local section, groups = "cpesim" .. saved_slot, {}
	local freq = uci:get("cpecfg", section, "custom_freq") == "1" and uci:get("cpecfg", section, "freq") or ""
	for item in (freq or ""):gmatch("[^,]+") do
		local name, bands = item:match("^([a-z]+)%-(.*)$")
		if name then groups[name] = bands end
	end
	local saved_policy = uci:get("cpecfg", section, "mobility")
	local saved_mode = uci:get("cpecfg", section, "mode")
	settings = { slot = saved_slot, policy = ({ ["0"] = true, ["1"] = true, ["2"] = true })[saved_policy] and saved_policy or "2",
		mode = ({ auto = true, sa_only = true, nsa = true, lte = true })[saved_mode] and saved_mode or "auto",
		nr_bands = groups.nr or groups.sa or "", lte_bands = groups.lte or "", application_state = "unknown_physical_slot" }
end
local m = SimpleForm("c8_modem", translate("蜂窝设置"))
m.reset, m.submit = false, false
local intro = m:section(SimpleSection)
intro.template = "myos_c8_modem/settings_intro"
intro.cache_state = snapshot.cache_state

local connection = m:section(SimpleSection, translate("SIM 与连接"))
local current = connection:option(DummyValue, "current_sim", translate("物理卡槽"))
current.default = labels[physical] or translate("未知")
local model = connection:option(DummyValue, "model", translate("模组型号"))
model.default = device.model or "--"
local slot = connection:option(ListValue, "slot", translate("目标 SIM 卡槽"))
for _, id in ipairs({ "1", "2", "3", "4" }) do slot:value(id, labels[id]) end
slot.default = physical or settings.slot or "1"
local switch = connection:option(Button, "switch_sim", translate("手动切卡"))
switch.inputtitle, switch.inputstyle = translate("切换到所选 SIM"), "action"
switch.description = translate("切卡后无网络，可先切回官方系统恢复出厂设置，再返回 OpenWrt 重新设置 SIM 卡。")
local reconnect = connection:option(Button, "reconnect", translate("数据连接"))
reconnect.inputtitle, reconnect.inputstyle = translate("重拨数据会话"), "action"

local ethernet = m:section(SimpleSection, translate("IP 透传") .. " / " .. (labels[physical or settings.slot] or labels["1"]))
local passthrough = ethernet:option(Flag, "ip_passthrough", translate("启用 IP 透传"))
local cursor = require("luci.model.uci").cursor()
local saved_sim = physical or settings.slot or "1"
passthrough.default = cursor:get("network", "cpe", "ippass") ~= "0"
	and cursor:get("cpecfg", "cpesim" .. saved_sim, "ippass") ~= "0" and "1" or "0"
passthrough.rmempty = false
passthrough.description = translate("默认开启。开启后路由器获取蜂窝地址；关闭后由模组分配内网地址。修改在设备下次启动时生效。")
local pt = snapshot.passthrough or {}
local pt_fresh = fresh and pt.stale == false and type(pt.enabled) == "boolean"
local pt_label = not pt_fresh and translate("状态未知") or pt.enabled == false and translate("关闭（模组路由）")
	or pt.ipv4_verified == true and translate("已透传") or pt.ipv4_verified == false and translate("已开启，地址未透传")
	or translate("已开启，等待地址核验")
if pt_fresh and pt.pending then pt_label = pt_label .. translate("；新配置待重启生效") end
local actual = ethernet:option(DummyValue, "ip_passthrough_state", translate("当前状态"))
actual.default = pt_label
local carrier = ethernet:option(DummyValue, "carrier_ipv4", translate("运营商 IPv4"))
carrier.default = pt_fresh and pt.carrier_ipv4 ~= "" and pt.carrier_ipv4 or "--"
local save_ethernet = ethernet:option(Button, "save_passthrough", translate("保存透传设置"))
save_ethernet.inputtitle, save_ethernet.inputstyle = translate("保存透传设置"), "apply"

local radio = m:section(SimpleSection, translate("网络策略") .. " / " .. (labels[settings.slot] or labels["1"]))
local application = radio:option(DummyValue, "application", translate("策略状态"))
application.default = settings.application_state == "unknown_physical_slot"
	and translate("保存配置；物理卡槽未知") or translate("保存配置；硬件状态未采样")
local policy = radio:option(ListValue, "policy", translate("锁频策略"))
policy:value("2", translate("无偏好"))
policy:value("1", translate("智能"))
policy:value("0", translate("锁定"))
policy.default, policy.widget = settings.policy or "2", "radio"
local mode = radio:option(ListValue, "mode", translate("网络模式"))
mode:value("auto", translate("自动"))
mode:value("sa_only", translate("5G SA"))
mode:value("nsa", translate("5G NSA"))
mode:value("lte", translate("4G LTE"))
mode.default = settings.mode or "auto"
for _, name in ipairs({ "nr", "lte" }) do
	local field = radio:option(Value, name .. "_bands", translate(name == "nr" and "NR 频段" or "LTE 频段"))
	field.default = settings[name .. "_bands"] or ""
	function field.validate(self, value)
		if not m:formvalue("cbid.c8_modem.1.save_policy") or value == "" or value:match("^[%d: ]+$") then return value end
		return nil, translate("频段格式错误")
	end
end
local save = radio:option(Button, "save_policy", translate("应用策略"))
save.inputtitle, save.inputstyle = translate("保存无线策略"), "apply"

function m.handle(self, state, data)
	if state ~= FORM_VALID or require("luci.http").getenv("REQUEST_METHOD") ~= "POST" then return true end
	local action, count = nil, 0
	for name, value in pairs({ switch_sim = "select_sim", reconnect = "reconnect", save_policy = "settings", save_passthrough = "ip_passthrough" }) do
		if data[name] then action, count = value, count + 1 end
	end
	if count ~= 1 then self.errmessage = translate("请选择一项操作"); return true end
	local request = { action = action }
	if action == "select_sim" then
		request.slot, request.origin = tonumber(data.slot), "module_settings"
	elseif action == "settings" then
		request.policy, request.mode = data.policy, data.mode
		request.nr_bands, request.lte_bands = data.nr_bands or "", data.lte_bands or ""
	elseif action == "ip_passthrough" then
		request.enabled = data.ip_passthrough == "1"
	end
	local result = modem.request(request)
	if result.ok == true then
		if action == "ip_passthrough" then
			self.message = result.restart_required and translate("透传配置已保存；重启设备后生效，当前连接继续使用现有模式。")
				or translate("透传配置已保存；模组已处于所选模式。")
		else
			self.message = action == "settings" and translate("策略已保存；模组回读通过") or translate("操作完成")
		end
	else
		self.errmessage = result.error or translate("操作失败")
	end
	return true
end
return m
