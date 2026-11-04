module("luci.controller.zz_mwrt_modem", package.seeall)

function index()
    for _, action in ipairs({"simstat", "simslots", "simset", "wlbridge_scan", "wlbridge_status", "wlbridge_join", "wlbridge_leave"}) do
        entry({"admin", "modem", action}, call("action_" .. action)).leaf = true
    end
    entry({"admin", "modem", "wifi_get"}, call("action_wifi_get")).leaf = true
    entry({"admin", "modem", "wifi_set"}, call("action_wifi_set")).leaf = true
    local mwrt = require("luci.model.uci").cursor():get("luci", "main", "mediaurlbase") == "/luci-static/argon"
    if not mwrt then return end
    local home = entry({"admin", "modeminfo"}, template("zmode/modem_info"), _("首页"), 2)
    home.dependent = false
    home.leaf = false
    entry({"admin", "modeminfo", "cellular_data"}, alias("admin", "modeminfo")).leaf = true
    entry({"admin", "modeminfo", "cellx"}, call("action_cellx")).leaf = true
    local data = entry({"admin", "modeminfo", "router3d"}, call("action_router3d"))
    data.leaf = true
    data.sysauth = false
end

function action_router3d()
    local out = luci.sys.exec("/www/cgi-bin/router3d-data") or ""
    luci.http.prepare_content("application/json")
    luci.http.header("Cache-Control", "no-store")
    luci.http.write(out:match("\r\n\r\n(.*)") or out:match("\n\n(.*)") or "{}")
end

function action_mwrtinit()
	local rv ={}
	local p = luci.http.formvalue("p")
	local set = luci.http.formvalue("set")
	local fixed = "txt"
	port= string.gsub(p, "\"", "~")
	rv["at"] = fixed 
	rv["port"] = port
	local file_exist = nixio.fs.access("/usr/bin/modemflag.conf")
	local handle = io.popen("lsusb")
    local result = handle:read("*a") or ""
    handle:close()

    if result:find("5700") then
		if file_exist then
			rv["rv"] = "OK"
		else
			rv["rv"] = "NO"
		end
	else
		rv["rv"] = "OK"
    end
	luci.http.prepare_content("application/json")
	luci.http.write_json(rv)
end

function action_mwrtco()
	local source_file = "/usr/share/modem/cascade.cssdf"
	local target_file = "/www/luci-static/argon/css/cascade.css"
	local command4 = string.format("cp %s %s", source_file, target_file)
	--os.execute(command4)
	local p = luci.http.formvalue("p")
	port= string.gsub(p, "\"", "~")
	local S = port
	local command = string.format("sed -i 's/--primary:[^;]*/--primary:%s;/' /www/luci-static/argon/css/cascade.css", S)
	os.execute(command)
	local command2 = string.format("sed -i 's/--bar-bg:[^;]*/--bar-bg:%s;/' /www/luci-static/argon/css/cascade.css", S)
	os.execute(command2)
	os.execute("/etc/init.d/uhttpd restart")
end

function action_mwrtdf()
	local source_file = "/usr/share/modem/cascade.cssdf"
	local target_file = "/www/luci-static/argon/css/cascade.css"
	local command = string.format("cp %s %s", source_file, target_file)
	os.execute(command)
	os.execute("/etc/init.d/uhttpd restart")
end

function action_mwrtpk()
	local source_file = "/usr/share/modem/cascade.csspk"
	local target_file = "/www/luci-static/argon/css/cascade.css"
	local command = string.format("cp %s %s", source_file, target_file)
	os.execute(command)
	os.execute("/etc/init.d/uhttpd restart")
end

function action_mwrtgr()
	local source_file = "/usr/share/modem/cascade.cssgr"
	local target_file = "/www/luci-static/argon/css/cascade.css"
	local command = string.format("cp %s %s", source_file, target_file)
	os.execute(command)
	os.execute("/etc/init.d/uhttpd restart")
end

function action_mwrtor()
	local source_file = "/usr/share/modem/cascade.cssor"
	local target_file = "/www/luci-static/argon/css/cascade.css"
	local command = string.format("cp %s %s", source_file, target_file)
	os.execute(command)
	os.execute("/etc/init.d/uhttpd restart")
end

function action_smscs()
	local rv ={}
	local file
	local p = luci.http.formvalue("p")
	local set = luci.http.formvalue("set")
	fixed = set
	port= string.gsub(p, "\"", "~")
	rv["at"] = fixed 
	rv["port"] = port

	os.execute("/usr/share/modem/smsc.sh \'" .. port .. "\' \'" .. fixed .. "\'")
	smsc = "/tmp/smsc.at"
	file = io.open(smsc, "r")
	if file ~= nil then
		rv["smsc"] = file:read("*all")
		file:close()
	else
		rv["smsc"] = " "
	end
	os.execute("/usr/share/modem/rsmsc.sh")
	luci.http.prepare_content("application/json")
	luci.http.write_json(rv)
end

-- 读短信,返回结构化 JSON。
--
-- action_smscs 回的是给老页面拼字符串用的文本块,发件人/时间/内容全靠行号切,
-- 已读未读更是塞不进去。这里直接用 pdu_decoder --json,它带 +CMGL 的 stat 位
-- (0=未读 1=已读),顺带把长短信的各段合成一条。AT+CPMS? 给出占用格数。
function action_smslist()
	local jsonc = require "luci.jsonc"
	local pipe = io.popen(
		"/usr/bin/sendat 1 at+cmgl=4 20 2>/dev/null | /usr/bin/pdu_decoder --json 2>/dev/null")
	local decoded = pipe and jsonc.parse(pipe:read("*all") or "") or nil
	if pipe then pipe:close() end

	local rv = { ok = false, messages = {} }
	if type(decoded) == "table" and decoded.ok then
		rv.ok = true
		rv.messages = decoded.messages or {}
	end

	-- 读不到容量不算失败,列表照常显示,只是不画那个占用条。
	local storage = io.popen("/usr/bin/sendat 1 'AT+CPMS?' 5 2>/dev/null")
	if storage then
		local used, total = (storage:read("*all") or ""):match('%+CPMS:%s*"[^"]*",(%d+),(%d+)')
		storage:close()
		rv.used = tonumber(used)
		rv.total = tonumber(total)
	end

	luci.http.prepare_content("application/json")
	luci.http.write_json(rv)
end

-- 删短信。idx 是逗号分隔的存储槽位号(长短信有好几个),或者 all 删全部。
-- 一条命令一个槽位:模组不收槽位列表。从大到小删,免得剩下的槽位号在中途挪位。
function action_smsdel()
	local util = require "luci.util"
	local idx = luci.http.formvalue("idx") or ""
	local rv = { ok = false, error = "无效的短信编号" }

	if idx == "all" then
		os.execute("/usr/bin/sendat 1 'AT+CMGD=0,4' 30 >/dev/null 2>&1")
		rv = { ok = true }
	elseif idx:match("^%d+[%d,]*$") then
		local slots = {}
		for slot in idx:gmatch("%d+") do slots[#slots + 1] = tonumber(slot) end
		table.sort(slots, function(left, right) return left > right end)
		for _, slot in ipairs(slots) do
			os.execute("/usr/bin/sendat 1 " ..
				util.shellquote("AT+CMGD=" .. slot) .. " 10 >/dev/null 2>&1")
		end
		rv = { ok = #slots > 0, error = #slots > 0 and nil or "无效的短信编号" }
	end

	luci.http.prepare_content("application/json")
	luci.http.write_json(rv)
end

-- 发短信。POST 而不是 GET: 一条 2000 字的中文短信 urlencode 完就是几 KB,塞不进 URL。
-- 真正的编码和发送在 /usr/bin/smssend 里(pdul.lua 编 PDU + 单行 AT+CMGS),
-- 这个模组不认标准的 '>' 提示符握手,所以 sms_tool_q 那条路在这台机器上发不出去。
function action_smssend()
	local util = require "luci.util"
	local jsonc = require "luci.jsonc"
	local number = luci.http.formvalue("number") or ""
	local text = luci.http.formvalue("text") or ""
	local rv = { ok = false, error = "号码或内容不合法" }

	if number:match("^%+?[%d%s%-()]+$") and #number <= 32 and
	   text ~= "" and #text <= 2048 and not text:find("%z") then
		local pipe = io.popen("/usr/bin/smssend " .. util.shellquote(number) ..
			" " .. util.shellquote(text) .. " 2>/dev/null")
		local output = pipe and pipe:read("*all") or ""
		if pipe then pipe:close() end
		local decoded = jsonc.parse(output)
		if type(decoded) == "table" then
			rv = decoded
		else
			rv = { ok = false, error = "发送工具没有返回结果" }
		end
	end

	luci.http.prepare_content("application/json")
	luci.http.write_json(rv)
end


function action_send_atcmd()
	local rv ={}
	local file
	local p = luci.http.formvalue("p")
	local set = luci.http.formvalue("set")
	--fixed = string.gsub(set, "\"", "~")
	fixed = set
	port= string.gsub(p, "\"", "~")
	rv["at"] = fixed 
	rv["port"] = port

	os.execute("/usr/share/modem/atcmd.sh \'" .. port .. "\' \'" .. fixed .. "\'")
	result = "/tmp/result.at"
	file = io.open(result, "r")
	if file ~= nil then
		rv["result"] = file:read("*all")
		file:close()
	else
		rv["result"] = " "
	end
	os.execute("/usr/share/modem/delatcmd.sh")
	luci.http.prepare_content("application/json")
	luci.http.write_json(rv)
end

function action_get_csq()
	local file = io.open("/tmp/modconf.conf", "r")
	if file then
		local content = file:read("*all")
		file:close()
		if content and string.find(content, "RM520") then
			rm520_rm500U()			
		elseif content and string.find(content, "RM500U") then
			rm520_rm500U()	
		elseif content and string.find(content, "MT5700") then	
			MT5700()
		end
	end
end

function action_get_if()
    local fs = require "nixio.fs"
    local uci = require "luci.model.uci".cursor()
    local http = require "luci.http"
    local rv = {}

    -- 获取无线 SSID
    local ssid_24g, ssid_5g

    uci:foreach("wireless", "wifi-iface", function(s)
        if s.device == "MT7981_1_1" and not ssid_24g then
            ssid_24g = s.ssid
        elseif s.device == "MT7981_1_2" and not ssid_5g then
            ssid_5g = s.ssid
        end
    end)

    rv["2.4G"] = ssid_24g or "未知"
    rv["5G"] = ssid_5g or "未知"

    -- CPU温度
    local cpu_temp_path = "/sys/class/thermal/thermal_zone0/temp"
    local temp = fs.readfile(cpu_temp_path)
    if temp then
        temp = tonumber(temp) / 1000
        rv["tmp"] = string.format("%.1f", temp)
    else
        rv["tmp"] = "未知"
    end

    -- 风扇转速
    local fanspeed = fs.readfile("/usr/bin/fanspeed.conf")
    rv["fansp"] = fanspeed and fanspeed:gsub("\n", "") or "未知"

    -- 运行时间
    local uptime = fs.readfile("/proc/uptime")
    if uptime then
        local sec = tonumber(uptime:match("^(%d+%.?%d*)"))
        if sec then
            local h = math.floor(sec / 3600)
            local m = math.floor((sec % 3600) / 60)
            local s = math.floor(sec % 60)
            rv["uptime"] = string.format("%02d:%02d:%02d", h, m, s)
        else
            rv["uptime"] = "未知"
        end
    else
        rv["uptime"] = "未知"
    end

    http.prepare_content("application/json")
    http.write_json(rv)
end


function MT5700()
	os.execute("/usr/share/modem/zinfo_mt5700.sh")
    local file, file2
    local stat = "/tmp/cpe_cell.file"
    local stat2 = "/tmp/stsss_u.file"
    file = io.open(stat, "r")
    file2 = io.open(stat2, "r")
	local rv ={}
	rv["stsss"] = file:read("*line")
	rv["modem"] = file:read("*line")
	rv["conntype"] = file:read("*line")
	rv["firmware"] = file:read("*line")
	rv["temper"] = file:read("*line")
	rv["date"] = file:read("*line")
	--------------------------------
	rv["simsel"] = file:read("*line")
	rv["cops"] = file:read("*line")
	rv["imei"] = file:read("*line")
	rv["imsi"] = file:read("*line")
	rv["iccid"] = file:read("*line")
	rv["phone"] = file:read("*line")
	--------------------------------
	rv["mode"] = file:read("*line")
	rv["per"] = file:read("*line")
	rv["rssi"] = file:read("*line")
	rv["rsrq"] = file:read("*line")
	rv["rscp"] = file:read("*line")
	rv["sinr"] = file:read("*line")
	-------------------------------
	rv["mcc"] = file:read("*line")
	rv["lac"] = file:read("*line")
	rv["cid"] = file:read("*line")
	rv["band"] = file:read("*line")
	rv["rfcn"] = file:read("*line")
	rv["pci"] = file:read("*line")
	rv["apn"] = file:read("*line")
	rv["down"] = file:read("*line")
	rv["up"] = file:read("*line")
	rv["qci"] = file:read("*line")
	rv["zbjh"] = file:read("*line")
	rv["r2cc"] = file:read("*line")
	rv["r3cc"] = file:read("*line")
	--------------------------------
	rv["cell1"] = file:read("*line")
	rv["cell2"] = file:read("*line")
	rv["cell3"] = file:read("*line")
	rv["cell4"] = file:read("*line")
	rv["cell5"] = file:read("*line")
-------------------------------
	luci.http.prepare_content("application/json")
	luci.http.write_json(rv)
end



-- 结构化小区信息。纯读缓存:采集器 /usr/bin/mwrt-cellinfo 由 cron 每分钟跑一次,
-- 这里只把它的 JSON 原样转出去,外加一个 age 字段让前端能标"数据已过期"。
--
-- 缓存不存在时(刚开机、非 5700 模组、采集器还没跑过第一轮)返回
-- {"supported":false},前端据此整段隐藏,而不是显示一堆空卡片。
function action_cellx()
	local CACHE = "/tmp/mwrt_cellinfo.json"
	local fs = require "nixio.fs"

	luci.http.prepare_content("application/json")

	-- 非 5700 模组直接短路:其他模组没有 ^HFREQINFO / ^MONNC 这些厂商命令,
	-- 这几个板块在它们身上没有意义(用户的要求就是"其他模组不需要")。
	local mod = fs.readfile("/tmp/modconf.conf") or ""
	if mod == "" then
		local uci = require("luci.model.uci").cursor()
		uci:foreach("qmodem", "modem-device", function(s) mod = mod .. " " .. (s.name or "") end)
	end
	if not mod:find("5700") then
		luci.http.write_json({ supported = false, reason = "not5700" })
		return
	end

	local raw = fs.readfile(CACHE)
	if not raw or #raw < 2 then
		-- 第一次访问时缓存可能还没生成。踢一脚采集器(后台,不等它),
		-- 这一轮先如实说"还没有数据",下一轮前端自然就拿到了。
		os.execute("(/usr/bin/mwrt-cellinfo >/dev/null 2>&1 &) &")
		luci.http.write_json({ supported = false, reason = "nocache" })
		return
	end

	local jsonc = require "luci.jsonc"
	local data = jsonc.parse(raw)
	if type(data) ~= "table" then
		luci.http.write_json({ supported = false, reason = "badcache" })
		return
	end

	data.supported = true
	local st = fs.stat(CACHE)
	if st and st.mtime then
		data.age = os.time() - st.mtime
	end
	-- 采集器只在 cron 的一分钟节拍上跑。如果缓存已经明显过期(设备刚休眠醒来、
	-- cron 被停过),顺手触发一次异步刷新,但仍然把手上这份旧数据先给出去 ——
	-- 前端有 age 可以自己决定显不显示"数据滞后"。
	if (data.age or 0) > 180 then
		os.execute("(/usr/bin/mwrt-cellinfo >/dev/null 2>&1 &) &")
	end

	luci.http.write_json(data)
end

local MAX_SELECTOR = "/etc/init.d/c2000max-sim-select"
local MAX_SIM_LABELS = { ["1"] = "外置SIM1卡槽", ["2"] = "外置SIM2卡槽", ["3"] = "内置卡" }
local function max_sim_status()
    local raw = luci.sys.exec(MAX_SELECTOR .. " status_json 2>/dev/null") or ""
    local state = require("luci.jsonc").parse(raw)
    return type(state) == "table" and state or {}
end

function action_simstat()
    local state = max_sim_status()
    local selected = state.actual or require("luci.model.uci").cursor():get("cpesel", "sim", "cur") or ""
    local ready = state.cpin == "READY"
    luci.http.prepare_content("application/json")
    luci.http.write_json({
        busy = state.busy and 1 or 0,
        code = ready and "12" or "",
        slot = MAX_SIM_LABELS[selected] or "",
        text = state.busy and "正在切换卡槽，请稍候" or ready and "SIM卡正常工作" or state.message or "正在读取卡状态",
        iccid = "",
        error = state.stage == "error" and state.message or nil
    })
end

function action_simslots()
    local state = max_sim_status()
    luci.http.prepare_content("application/json")
    luci.http.write_json({
        supported = require("nixio.fs").access(MAX_SELECTOR),
        is_c2000 = true,
        current = state.actual or require("luci.model.uci").cursor():get("cpesel", "sim", "cur") or "",
        slots = {
            { v = "1", label = MAX_SIM_LABELS["1"] },
            { v = "2", label = MAX_SIM_LABELS["2"] },
            { v = "3", label = MAX_SIM_LABELS["3"] }
        }
    })
end

function action_simset()
    if not require("luci.dispatcher").test_post_security() then return end
    luci.http.prepare_content("application/json")
    local target = luci.http.formvalue("slot") or ""
    if not MAX_SIM_LABELS[target] then
        return luci.http.write_json({ ok = false, error = "该卡槽在本机不可用" })
    end
    local state = max_sim_status()
    if state.busy then
        return luci.http.write_json({ ok = false, error = "正在切卡，请等待当前任务完成" })
    end
    if state.actual == target then
        return luci.http.write_json({ ok = true, noop = true, current = target })
    end
    local rc = os.execute(MAX_SELECTOR .. " select_sim " .. target .. " >/dev/null 2>&1")
    luci.http.write_json({ ok = rc == 0, current = target, error = rc ~= 0 and "切卡任务提交失败" or nil })
end

local function wifi_find(band)
	local uci = require "luci.model.uci".cursor()
	local dev
	uci:foreach("wireless", "wifi-device", function(s)
		if not dev and s.band == band then dev = s[".name"] end
	end)
	if not dev then return nil, nil, "找不到 " .. band .. " 频段的无线设备" end

	local names = {}
	uci:foreach("wireless", "wifi-iface", function(s)
		if s.device == dev and (s.mode or "ap") == "ap" then
			names[#names + 1] = s[".name"]
		end
	end)
	if #names == 0 then return nil, nil, "该频段没有可配置的无线接口" end
	table.sort(names)
	return dev, names[1], nil
end

-- 读当前无线配置。GET,只读。
-- **不回传 key。** 现有无线密码不出现在页面上;前端那个密码框留空即表示不修改。
function action_wifi_get()
	local uci = require "luci.model.uci".cursor()
	local band = luci.http.formvalue("band")

	luci.http.prepare_content("application/json")
	if band ~= "2g" and band ~= "5g" then
		luci.http.write_json({ ok = false, error = "频段参数不合法" })
		return
	end

	local dev, iface, err = wifi_find(band)
	if not iface then
		luci.http.write_json({ ok = false, error = err })
		return
	end

	luci.http.write_json({
		ok = true,
		band = band,
		device = dev,
		ssid = uci:get("wireless", iface, "ssid") or "",
		encryption = uci:get("wireless", iface, "encryption") or "none",
		-- 设备整体关闭 或 这个接口单独关闭,都算关闭
		disabled = (uci:get("wireless", dev, "disabled") == "1"
			or uci:get("wireless", iface, "disabled") == "1")
	})
end

-- 写无线配置。**必须 POST 且带 CSRF token** —— test_post_security() 两件都查,
-- 不通过它自己回 405/403 并且返回 false。
-- 改 SSID/密码是能把用户锁在网络外面的操作,不能让别的站点拿着会话 cookie
-- 用一个 <img src> 式的 GET 就触发。
function action_wifi_set()
	-- dispatcher 此刻必然已加载(它正在调度这个请求), require 走的是缓存,
	-- 但显式写出来才不依赖 package.seeall 碰巧能看到那个全局。
	-- test_post_security 内部用的 http/context 是它自己的闭包 upvalue,
	-- 跨模块调用照样拿得到。
	local dsp = require "luci.dispatcher"
	if not dsp.test_post_security() then return end

	local uci = require "luci.model.uci".cursor()
	local band = luci.http.formvalue("band")
	local ssid = luci.http.formvalue("ssid") or ""
	local enc = luci.http.formvalue("encryption") or ""
	local key = luci.http.formvalue("key") or ""

	luci.http.prepare_content("application/json")

	-- 前端也校验了一遍,但那只是为了少跑一个来回;真正的闸门在这里。
	if band ~= "2g" and band ~= "5g" then
		luci.http.write_json({ ok = false, error = "频段参数不合法" })
		return
	end
	-- SSID 按**字节**算长度:这是 802.11 的硬限制(SSID 字段 32 字节),
	-- 不是字符数 —— 中文一个字 3 字节,11 个汉字就到顶了。
	if ssid == "" or #ssid > 32 then
		luci.http.write_json({ ok = false, error = "网络名称需要 1 - 32 个字节(中文每字约 3 字节)" })
		return
	end
	-- 控制字符会把 uci 配置写坏,也不可能是用户真想要的名字
	if ssid:find("[%c]") then
		luci.http.write_json({ ok = false, error = "网络名称不能包含控制字符" })
		return
	end
	-- 白名单,不是黑名单:加密方式会被原样写进 uci 并交给 hostapd,
	-- 只放行确实支持的那几种写法。
	local ENC_OK = {
		["none"] = true, ["psk"] = true, ["psk2"] = true,
		["psk-mixed"] = true, ["sae"] = true, ["sae-mixed"] = true
	}
	if not ENC_OK[enc] then
		luci.http.write_json({ ok = false, error = "不支持的加密方式" })
		return
	end
	if enc ~= "none" and key ~= "" and (#key < 8 or #key > 63) then
		luci.http.write_json({ ok = false, error = "无线密码需要 8 - 63 个字符" })
		return
	end

	local dev, iface, err = wifi_find(band)
	if not iface then
		luci.http.write_json({ ok = false, error = err })
		return
	end

	-- 改成加密但没给密码,而原来也没有密码 —— 这会落到一个"加密却无密码"的
	-- 配置上,hostapd 起不来,无线直接消失。挡在写之前。
	local oldkey = uci:get("wireless", iface, "key") or ""
	if enc ~= "none" and key == "" and oldkey == "" then
		luci.http.write_json({ ok = false, error = "该频段当前没有密码, 改为加密时必须设置一个" })
		return
	end

	uci:set("wireless", iface, "ssid", ssid)
	uci:set("wireless", iface, "encryption", enc)
	if enc == "none" then
		-- 开放网络留着一个 key 没有意义,而且下次切回加密时会悄悄复用旧密码
		uci:delete("wireless", iface, "key")
	elseif key ~= "" then
		uci:set("wireless", iface, "key", key)
	end
	-- key == "" 且 enc ~= "none": 保留原密码不动,这就是前端"留空表示不修改"

	uci:commit("wireless")

	-- **异步重启无线。** 同步跑 `wifi reload` 的话:用户大概率正通过这个频段的
	-- WiFi 访问本页面,无线一断 HTTP 响应就发不出去,浏览器只会看到请求失败,
	-- 分不清是没保存成功还是只是断线。延后一秒再 reload,先把这个 JSON 回出去。
	os.execute("(sleep 1; /sbin/wifi reload) >/dev/null 2>&1 &")

	luci.http.write_json({ ok = true, band = band, device = dev, ssid = ssid })
end

local WLB_NET = "wwan"
local WLB_SID = "mwrt_wlbridge"
-- 无线网桥事件文件: 写 "<epoch>|up|<ssid>" / "<epoch>|down|<ssid>", 供右上角
-- 通知中心(rpcd luci.mwrt-notice)当"一次性事件"读出来显示一小会儿。两边路径一致。
local WLB_EVENT = "/tmp/mwrt-wlbridge.event"

-- 写一条桥接事件。kind = "up" / "down"。ssid 里的竖线/换行剔掉(分隔符安全),
-- 用 nixio.fs.writefile 直接落地(不 fork、不经 shell, 免注入)。
local function wlb_write_event(kind, ssid)
	local fs = require "nixio.fs"
	local s = tostring(ssid or ""):gsub("[|\r\n]", " ")
	local ts = os.time()
	fs.writefile(WLB_EVENT, string.format("%d|%s|%s\n", ts, kind, s))
end

-- 按 band('2g'/'5g')拿 { device=wifi-device名, phy=ra0/rai0, netdev=apcli0/apclii0 }。
-- netdev 由 phy 推: ra0→apcli0, rai0→apclii0(prefix + idx0, 和 l1util apcli_ifname 一致)。
local function wlb_radio(band)
	local uci = require "luci.model.uci".cursor()
	local dev, phy
	uci:foreach("wireless", "wifi-device", function(s)
		if not dev and s.band == band then dev = s[".name"]; phy = s.phy end
	end)
	if not dev then return nil end
	-- ra0 → apcli0 ; rai0 → apclii0(把 phy 里 ra 后面那段插 "cli")
	local netdev
	if phy == "ra0" then netdev = "apcli0"
	elseif phy == "rai0" then netdev = "apclii0"
	else
		-- 兜底: raX → apcliX、raiX → apcliiX
		local base, idx = (phy or ""):match("^(ra[i]?)(%d+)$")
		if base then netdev = base .. "cli" .. idx else netdev = "apcli0" end
	end
	return { device = dev, phy = phy, netdev = netdev }
end

-- 把 iwinfo scan 的 encryption 对象映射成 uci encryption 值(与 mtwifi enc2dat / wireless.js 同口径)。
local function wlb_enc(e)
	if type(e) ~= "table" or not e.enabled then return "none" end
	local auth = e.authentication or {}
	local has = function(t, v) for _, x in ipairs(t) do if x == v then return true end end return false end
	if has(auth, "sae") then
		-- 对端是 WPA3(或 WPA2/WPA3 过渡), 都用 sae(mtwifi sae 能连过渡 AP)
		return "sae"
	end
	if has(auth, "psk") then
		local wpa = e.wpa or {}
		local w1, w2 = has(wpa, 1), has(wpa, 2)
		if w1 and w2 then return "psk-mixed" end
		if w2 then return "psk2" end
		if w1 then return "psk" end
		return "psk2"
	end
	-- 只有 WEP 或未知加密: 不支持桥接(返回空让前端标"不支持")
	if e.wep then return "wep" end
	return "unknown"
end

-- 字符串是否是「干净可显示」的 UTF-8 SSID(过滤隐藏/乱码 AP)。
-- 严格校验 UTF-8: ASCII 可见字符直接过; 多字节必须是合法 UTF-8 续字节序列。
-- 非法字节(单独的 0x80-0xBF、0xFE/0xFF、DEL 0x7F、控制符)→ 判脏。
local function wlb_utf8_ok(s)
	if not s or s == "" then return false end
	if s:find("\239\191\189") then return false end  -- U+FFFD 替换符
	local i, n = 1, #s
	while i <= n do
		local c = s:byte(i)
		if c < 0x20 or c == 0x7F then
			return false  -- 控制字符 / DEL
		elseif c < 0x80 then
			i = i + 1     -- ASCII 可见
		elseif c >= 0xC2 and c <= 0xDF then
			-- 2 字节: 后面 1 个 0x80-0xBF
			local c2 = s:byte(i + 1)
			if not c2 or c2 < 0x80 or c2 > 0xBF then return false end
			i = i + 2
		elseif c >= 0xE0 and c <= 0xEF then
			-- 3 字节(含中文): 后面 2 个 0x80-0xBF
			local c2, c3 = s:byte(i + 1), s:byte(i + 2)
			if not c2 or c2 < 0x80 or c2 > 0xBF or not c3 or c3 < 0x80 or c3 > 0xBF then return false end
			i = i + 3
		elseif c >= 0xF0 and c <= 0xF4 then
			-- 4 字节: 后面 3 个 0x80-0xBF
			local c2, c3, c4 = s:byte(i + 1), s:byte(i + 2), s:byte(i + 3)
			if not c2 or c2 < 0x80 or c2 > 0xBF or not c3 or c3 < 0x80 or c3 > 0xBF or not c4 or c4 < 0x80 or c4 > 0xBF then return false end
			i = i + 4
		else
			return false  -- 0x80-0xC1 单独出现、0xF5-0xFF 非法
		end
	end
	return true
end

function action_wlbridge_scan()
	local band = luci.http.formvalue("band")
	luci.http.prepare_content("application/json")
	if band ~= "2g" and band ~= "5g" then
		luci.http.write_json({ ok = false, error = "频段参数不合法" })
		return
	end
	local r = wlb_radio(band)
	if not r then
		luci.http.write_json({ ok = false, error = "找不到 " .. band .. " 频段的无线设备" })
		return
	end

	-- ubus call iwinfo scan {"device":"ra0"} —— 返回结构化结果, 不用解析 iwpriv。
	local jsonc = require "luci.jsonc"
	local out = luci.sys.exec("ubus call iwinfo scan '{\"device\":\"" .. r.phy .. "\"}' 2>/dev/null") or ""
	local parsed = jsonc.parse(out)
	local results = (type(parsed) == "table" and parsed.results) or {}

	local list = {}
	for _, b in ipairs(results) do
		local enc = wlb_enc(b.encryption)
		local ssid = b.ssid
		local ok = wlb_utf8_ok(ssid)
		-- 保留能连的(psk/psk2/psk-mixed/sae/none); wep/unknown 标出来但置灰不可点
		list[#list + 1] = {
			ssid = ok and ssid or "",
			hidden = (not ok),
			bssid = b.bssid,
			signal = b.signal,
			quality = b.quality,
			quality_max = b.quality_max,
			channel = b.channel,
			enc = enc,
			joinable = ok and (enc ~= "wep" and enc ~= "unknown"),
		}
	end
	-- 按信号质量降序
	table.sort(list, function(a, bb) return (a.quality or 0) > (bb.quality or 0) end)
	luci.http.write_json({ ok = true, band = band, netdev = r.netdev, list = list })
end

-- 读当前桥接状态: 有没有 sta iface、apcli 关联到哪、拿没拿到 IP、是否已设为主路由。
local function wlb_read_status()
	local uci = require "luci.model.uci".cursor()
	local fs = require "nixio.fs"
	local st = { bridged = false, band = "", ssid = "", bssid = "", ip = "",
		signal = nil, associated = false, primary_on = false, netdev = "" }

	-- 找我们建的 sta iface
	local sid
	uci:foreach("wireless", "wifi-iface", function(s)
		if s.mode == "sta" then sid = s[".name"]; return false end
	end)
	if not sid then return st end
	local dev = uci:get("wireless", sid, "device")
	st.ssid = uci:get("wireless", sid, "ssid") or ""
	-- device → band → netdev
	local band = uci:get("wireless", dev or "", "band") or ""
	st.band = band
	local r = band ~= "" and wlb_radio(band) or nil
	local netdev = r and r.netdev or "apcli0"
	st.netdev = netdev
	st.bridged = true

	-- 关联状态: iwconfig 的 Access Point(Not-Associated 为未连)
	local iw = luci.sys.exec("iwconfig " .. netdev .. " 2>/dev/null") or ""
	local ap = iw:match("Access Point:%s*([%x:]+)")
	st.associated = (ap ~= nil and ap:upper() ~= "NOT-ASSOCIATED" and ap ~= "00:00:00:00:00:00")
	if st.associated then st.bssid = ap:upper() end
	local essid = iw:match('ESSID:"([^"]*)"')
	if essid and essid ~= "" then st.ssid = essid end

	-- 拿到 IP 没(apcli netdev 上的 ipv4)
	local ipout = luci.sys.exec("ip -4 addr show dev " .. netdev .. " 2>/dev/null") or ""
	st.ip = ipout:match("inet%s+([%d%.]+)") or ""

	-- 是否已设为主路由(Mwrt-Assurance)
	local pif = uci:get("Mwrt-Assurance", "wan_failover", "primary_if") or ""
	local en = uci:get("Mwrt-Assurance", "wan_failover", "enabled") or "0"
	st.primary_on = (pif == netdev and en == "1")
	return st
end

function action_wlbridge_status()
	luci.http.prepare_content("application/json")
	luci.http.write_json(wlb_read_status())
end

function action_wlbridge_join()
	local dsp = require "luci.dispatcher"
	if not dsp.test_post_security() then return end
	luci.http.prepare_content("application/json")

	local band = luci.http.formvalue("band")
	local ssid = luci.http.formvalue("ssid") or ""
	local bssid = luci.http.formvalue("bssid") or ""
	local key = luci.http.formvalue("key") or ""
	local enc = luci.http.formvalue("enc") or ""
	local as_primary = (luci.http.formvalue("primary") == "1")

	if band ~= "2g" and band ~= "5g" then
		luci.http.write_json({ ok = false, error = "频段参数不合法" }); return
	end
	if ssid == "" or #ssid > 32 then
		luci.http.write_json({ ok = false, error = "上游网络名称不合法" }); return
	end
	local ENC_OK = { ["none"]=true, ["psk"]=true, ["psk2"]=true, ["psk-mixed"]=true, ["sae"]=true }
	if not ENC_OK[enc] then
		luci.http.write_json({ ok = false, error = "该网络的加密方式暂不支持桥接" }); return
	end
	if enc ~= "none" and (#key < 8 or #key > 63) then
		luci.http.write_json({ ok = false, error = "上游 WiFi 密码需要 8 - 63 个字符" }); return
	end

	local r = wlb_radio(band)
	if not r then
		luci.http.write_json({ ok = false, error = "找不到 " .. band .. " 频段的无线设备" }); return
	end

	local uci = require "luci.model.uci".cursor()

	-- 幂等: 先删掉所有已存在的 sta iface(本功能固定单桥接), 避免叠加 / 换频段残留。
	local to_del = {}
	uci:foreach("wireless", "wifi-iface", function(s)
		if s.mode == "sta" then to_del[#to_del + 1] = s[".name"] end
	end)
	for _, n in ipairs(to_del) do uci:delete("wireless", n) end

	-- 建 STA 客户端 iface
	uci:section("wireless", "wifi-iface", WLB_SID, {
		device = r.device,
		mode = "sta",
		network = WLB_NET,
		ssid = ssid,
		encryption = enc,
	})
	if enc ~= "none" then uci:set("wireless", WLB_SID, "key", key) end
	if bssid ~= "" and bssid:match("^[%x:]+$") then uci:set("wireless", WLB_SID, "bssid", bssid) end
	uci:commit("wireless")

	-- network.wwan = dhcp 接口(apcli netdev 由 netifd 自动挂进来)
	uci:delete("network", WLB_NET)
	uci:section("network", "interface", WLB_NET, { proto = "dhcp" })
	uci:commit("network")

	-- 把 wwan 加进 wan 防火墙区域(只增不删其它成员)
	uci:foreach("firewall", "zone", function(s)
		if s.name == "wan" then
			local nets = s.network or {}
			if type(nets) == "string" then nets = { nets } end
			local seen = false
			for _, n in ipairs(nets) do if n == WLB_NET then seen = true end end
			if not seen then nets[#nets + 1] = WLB_NET; uci:set("firewall", s[".name"], "network", nets) end
			return false
		end
	end)
	uci:commit("firewall")

	-- 设为主路由 + 开网关存活(可选, 默认开)。
	-- ⚠ 只改 primary_if, **不动 secondary_if** —— secondary 是原来的故障兜底口
	-- (通常是蜂窝 eth2)。早期版本把旧 primary 塞进 secondary, 结果把蜂窝兜底口
	-- 覆盖成了宽带口 eth1, 断开后"路由切换状态"里蜂窝一直显示离线(它在探 eth1)。
	-- 现在:桥接前的 primary_if / secondary_if / enabled 全存到 sta 节点, 断开时三者
	-- 原样还原, 谁都不越权覆盖。
	if as_primary then
		local old_pif = uci:get("Mwrt-Assurance", "wan_failover", "primary_if") or ""
		local old_sif = uci:get("Mwrt-Assurance", "wan_failover", "secondary_if") or ""
		local old_en  = uci:get("Mwrt-Assurance", "wan_failover", "enabled") or "0"
		uci:set("wireless", WLB_SID, "wlb_old_en", old_en)
		uci:set("wireless", WLB_SID, "wlb_old_pif", old_pif)
		uci:set("wireless", WLB_SID, "wlb_old_sif", old_sif)
		uci:commit("wireless")
		uci:set("Mwrt-Assurance", "wan_failover", "primary_if", r.netdev)
		uci:set("Mwrt-Assurance", "wan_failover", "enabled", "1")
		uci:set("Mwrt-Assurance", "wan_failover", "force_primary", "1")
		uci:commit("Mwrt-Assurance")
	end

	-- 异步应用: wifi reload 建 apcli + 关联, ifup wwan 起 DHCP。放后台, 不阻塞本响应
	-- (用户可能正用本机 WiFi 连着这页, 同步跑会把响应闷死)。
	os.execute("( wifi reload; sleep 2; /sbin/ifup " .. WLB_NET .. " ) >/dev/null 2>&1 &")

	-- 发一条"已接入"事件给右上角通知中心
	wlb_write_event("up", ssid)

	luci.http.write_json({ ok = true, netdev = r.netdev, band = band })
end

function action_wlbridge_leave()
	local dsp = require "luci.dispatcher"
	if not dsp.test_post_security() then return end
	luci.http.prepare_content("application/json")

	local uci = require "luci.model.uci".cursor()

	-- 记下当前桥接网卡, 好把网关存活主口还原
	local st = wlb_read_status()

	-- 删 sta iface 前, 先读出桥接时存下的"桥接前网关存活状态", 用于精确还原
	local old_en, old_pif, old_sif = nil, nil, nil
	uci:foreach("wireless", "wifi-iface", function(s)
		if s.mode == "sta" then
			old_en = s.wlb_old_en; old_pif = s.wlb_old_pif; old_sif = s.wlb_old_sif
		end
	end)

	-- 删 sta iface
	local dels = {}
	uci:foreach("wireless", "wifi-iface", function(s)
		if s.mode == "sta" then dels[#dels + 1] = s[".name"] end
	end)
	for _, n in ipairs(dels) do uci:delete("wireless", n) end
	uci:commit("wireless")

	-- 从 wan 区域移除 wwan + 删 network.wwan
	uci:foreach("firewall", "zone", function(s)
		if s.name == "wan" then
			local nets = s.network or {}
			if type(nets) == "string" then nets = { nets } end
			local keep = {}
			for _, n in ipairs(nets) do if n ~= WLB_NET then keep[#keep + 1] = n end end
			uci:set("firewall", s[".name"], "network", keep)
			return false
		end
	end)
	uci:commit("firewall")
	uci:delete("network", WLB_NET)
	uci:commit("network")

	-- 网关存活还原: 只有当前主口确实是桥接网卡时才动它(别人手动改过就不越权)。
	-- 把桥接前存下的 primary_if / secondary_if / enabled 三者原样还原 —— 尤其
	-- secondary_if 必须还原, 否则蜂窝兜底口会一直停在错的网卡上, 状态页显示离线。
	local pif = uci:get("Mwrt-Assurance", "wan_failover", "primary_if") or ""
	if pif == st.netdev and st.netdev ~= "" then
		local changed = false
		if old_pif and old_pif ~= "" and old_pif ~= st.netdev then
			uci:set("Mwrt-Assurance", "wan_failover", "primary_if", old_pif); changed = true
		else
			-- 存不到桥接前主口: 回落到 secondary 兜底口(不把 primary 留在已删的桥接网卡上)
			local sif = uci:get("Mwrt-Assurance", "wan_failover", "secondary_if") or ""
			if sif ~= "" and sif ~= st.netdev then
				uci:set("Mwrt-Assurance", "wan_failover", "primary_if", sif); changed = true
			end
		end
		-- secondary_if 原样还原(桥接时本不该动它, 这里兜底把早期版本搞乱的也纠回来)
		if old_sif and old_sif ~= "" and old_sif ~= st.netdev then
			uci:set("Mwrt-Assurance", "wan_failover", "secondary_if", old_sif); changed = true
		end
		-- 桥接前是关着的, 就还原成关(桥接是唯一把它打开的原因)
		if old_en ~= nil then
			uci:set("Mwrt-Assurance", "wan_failover", "enabled", old_en); changed = true
		end
		if changed then uci:commit("Mwrt-Assurance") end
	end

	os.execute("( wifi reload ) >/dev/null 2>&1 &")

	-- 发一条"已断开"事件给右上角通知中心(用拆桥前记下的 ssid)
	wlb_write_event("down", st.ssid)

	luci.http.write_json({ ok = true })
end
