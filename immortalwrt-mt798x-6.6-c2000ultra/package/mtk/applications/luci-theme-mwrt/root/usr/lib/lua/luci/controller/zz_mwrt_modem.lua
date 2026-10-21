module("luci.controller.zz_mwrt_modem", package.seeall)

function index()
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
    entry({"admin", "modem", "simstat"}, call("action_simstat")).leaf = true
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

function action_simstat()
	local out = luci.sys.exec("/usr/share/modem/simstat.sh 2>/dev/null") or ""
	luci.http.prepare_content("application/json")
	if out:match("^%s*{") then
		luci.http.write(out)
	else
		-- 脚本缺失或异常时给个结构一致的兜底,前端不用做特判
		luci.http.write_json({
			busy = 0, code = "", slot = "",
			iccid = "", text = "状态接口无响应"
		})
	end
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

