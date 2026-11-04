module("luci.controller.uuplugin", package.seeall)

function index()
	if not nixio.fs.access("/etc/config/uuplugin") then
		return
	end

	entry({"admin", "services", "uuplugin"}, cbi("uuplugin"), ("网易UU游戏加速器"), 99).dependent = true
	entry({"admin", "services", "uuplugin", "status"}, call("act_status")).leaf = true
end

function act_status()
	local e = {}
	local uci = require("luci.model.uci").cursor()
	local configured = uci:get("uuplugin", "uuplugin", "platform") or "openwrt-aarch64"
	local platform = nixio.fs.readfile("/tmp/uu/.platform") or ""
	local activate_raw = nixio.fs.readfile("/tmp/uu/activate_status") or ""
	local activate_lower = activate_raw:lower()
	local uu_status = activate_lower:match("uu_status%s*=%s*[\"']?(%d+)")
		or activate_lower:match("[\"']uu_status[\"']%s*:%s*[\"']?(%d+)")
		or activate_lower:match("^%s*(%d+)%s*$")
	local problems = {}
	if configured ~= "openwrt-aarch64" and configured ~= "h3c-nx30pro" then
		configured = "openwrt-aarch64"
	end
	platform = platform:gsub("[\r\n]+$", "")
	e.enabled = uci:get("uuplugin", "uuplugin", "enabled") == "1"
	e.running = luci.sys.call(
		"/usr/libexec/uuplugin-launcher --core-running >/dev/null 2>&1") == 0
	e.guardian = luci.sys.call(
		"/usr/libexec/uuplugin-launcher --guardian-running >/dev/null 2>&1") == 0
	e.uu_status = tonumber(uu_status)
	e.activated = e.uu_status ~= nil and e.uu_status > 0
	e.rules_ready = luci.sys.call(
		"/usr/libexec/uuplugin-launcher --rules-present >/dev/null 2>&1") == 0

	if (e.enabled or e.running) and platform ~= configured then
		problems[#problems + 1] = "核心平台尚未切换完成"
	end
	if (e.enabled or e.running) and not nixio.fs.access("/dev/net/tun") then
		problems[#problems + 1] = "缺少 /dev/net/tun"
	end
	if (e.enabled or e.running) and luci.sys.call("/usr/libexec/ip-full -4 route show default 2>/dev/null | grep -q '^default '") ~= 0 then
		problems[#problems + 1] = "缺少 IPv4 默认路由"
	end
	if e.running and not e.guardian then
		problems[#problems + 1] = "guardian 未运行"
	end
	if (e.enabled or e.running) and not nixio.fs.access("/tmp/uu/xtables-nft-multi") then
		problems[#problems + 1] = "官方 xtables 兼容组件不完整"
	end
	if e.running and platform == "h3c-nx30pro" then
		if not nixio.fs.access("/tmp/uu/h3c_info") then
			problems[#problems + 1] = "缺少 h3c_info"
		end

	end


	e.ready = e.running and e.guardian and e.activated and e.rules_ready
		and platform == configured and #problems == 0
	if not e.running then
		e.phase = "stopped"
	elseif e.ready then
		e.phase = "ready"
	elseif e.uu_status ~= nil or #problems > 0 then
		e.phase = "network_uninitialized"
	else
		e.phase = "core_running"
	end
	if e.running and e.uu_status == nil then
		problems[#problems + 1] = "等待 activate_status 或 APP 下发加速"
	elseif e.running and not e.activated then
		problems[#problems + 1] = "activate_status: uu_status=" .. tostring(e.uu_status)
	elseif e.running and not e.rules_ready then
		problems[#problems + 1] = "未检测到完整 UU 规则"
	end
	e.platform = platform
	e.configured_platform = configured
	e.health_detail = table.concat(problems, "；")
	e.status = nixio.fs.readfile("/tmp/uuplugin.state") or "已停用"
	e.status = e.status:gsub("[\r\n]+$", "")
	luci.http.prepare_content("application/json")
	luci.http.write_json(e)
end
