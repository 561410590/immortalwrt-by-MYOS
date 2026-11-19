module("luci.controller.myos_ultra_tools", package.seeall)

local phone = require("luci.model.myos_ultra_phone")

function index()
    local page = entry({"admin", "embedded", "phone"}, view("myos-new/ultra-phone"), "电话（实验性）", 3)
    page.acl_depends = {"luci-app-embed"}
    entry({"admin", "myos", "ultra_phone"}, call("phone_state")).leaf = true
    local control = entry({"admin", "myos", "ultra_phone_control"}, post("phone_control"))
    control.leaf = true
    control.acl_depends = {"luci-mod-network-config"}
    entry({"admin", "myos", "ultra_egress"}, post("egress")).leaf = true
end

local function reply(data)
    luci.http.header("Cache-Control", "no-store")
    luci.http.prepare_content("application/json")
    luci.http.write_json(data)
end

function phone_state()
    local port = phone.port()
    if not port then return reply({supported=false}) end
    local cache = phone.cached() or {}
    local age = os.time() - (cache.sampled or 0)
    local current, reason
    if cache.supported and age >= 0 and age <= 12 and luci.http.formvalue("fresh") ~= "1" then
        reason = cache.error
        if not reason then current = cache.calls end
    else
        current, reason = phone.calls(port)
    end
    local ims
    if luci.http.formvalue("ims") == "1" then
        local response, ok = phone.at(port, "AT^IMSSWITCH?")
        if ok then ims = tonumber(response:match("%^IMSSWITCH:%s*(%d+)")) end
    end
    reply({supported=true, calls=current, recent=cache.recent, ims=ims, error=reason})
end

local call_commands = { answer="ATA", hangup="AT+CHUP" }

function phone_control()
    local port = phone.port()
    if not port then return reply({ok=false, error="未找到 MT5700 的 AT 端口"}) end
    local action = luci.http.formvalue("action")
    local command = call_commands[action]
    if action == "dial" then
        local number = luci.http.formvalue("number") or ""
        if not number:match("^%+?[%d*#]+$") then
            return reply({ok=false, error="请输入号码，可使用数字、*、# 和开头的 +"})
        end
        command = "ATD" .. number .. ";"
    end
    if not command then return reply({ok=false, error="未知通话操作"}) end
    local response, ok = phone.at(port, command)
    if action == "hangup" and not ok then
        response, ok = phone.at(port, "ATH")
    end
    local error = response:match("(%+CME ERROR:%s*%d+)") or response:match("(NO CARRIER)") or
        response:match("(NO ANSWER)") or response:match("(BUSY)") or response:match("(ERROR)")
    if error then return reply({ok=false, error="模组返回 " .. error}) end
    if ok then return reply({ok=true}) end
    reply({ok=false, error="模组未确认，请查看当前通话状态"})
end

local targets = {
    baidu = "https://www.baidu.com/",
    tencent = "https://www.qq.com/",
    alibaba = "https://www.aliyun.com/",
    cloudflare = "https://www.cloudflare.com/",
    google = "https://www.google.com/",
    youtube = "https://www.youtube.com/"
}

local curl_errors = { [6]="DNS 解析失败", [7]="TCP 连接失败", [28]="请求超时",
    [35]="TLS 握手失败", [56]="连接中断", [60]="证书校验失败" }

function egress()
    local target = luci.http.formvalue("target")
    local family = luci.http.formvalue("family")
    local flag = family == "6" and "-6" or "-4"
    local curl = "/usr/bin/curl -sS --connect-timeout 3 --max-time 6 " .. flag
    if target == "public" then
        if family == "6" then
            local trace = luci.sys.exec(curl .. " https://www.cloudflare.com/cdn-cgi/trace 2>/dev/null") or ""
            local address = trace:match("[\r\n]ip=([%x:]+)")
            return reply({ok=address~=nil, address=address,
                error=not address and "未获取公网 IPv6 地址" or nil})
        end
        local body = luci.sys.exec(curl .. " https://myip.ipip.net 2>/dev/null") or ""
        local address = body:match("IP：%s*([%x:.]+)")
        local location = body:match("来自于：%s*([^\r\n]+)")
        return reply({ok=address~=nil, address=address, location=location,
            error=not address and "未获取公网地址" or nil})
    end
    local url = targets[target]
    if not url then return reply({ok=false, error="未知检测目标"}) end
    local format = "%{http_code} %{time_namelookup} %{time_connect} %{time_appconnect} %{time_starttransfer} %{local_ip}"
    local result = luci.sys.exec(curl .. " -I -o /dev/null -w '" .. format .. "' " .. url ..
        " 2>/dev/null; printf '\\n%d' \"$?\"") or ""
    local code, dns, tcp, tls, ttfb, local_ip, rc = result:match("^(%d+) ([%d.]+) ([%d.]+) ([%d.]+) ([%d.]+) (%S*)%s+(%d+)")
    if not rc then return reply({ok=false, error="未获取检测结果"}) end
    local function ms(s) return math.floor(tonumber(s) * 1000 + 0.5) end
    local connected = tonumber(tcp) > 0
    local encrypted = tonumber(tls) > 0
    rc = tonumber(rc)
    reply({ok=rc==0 and code~="000", code=tonumber(code), source=local_ip,
        dns=rc~=6 and ms(dns) or nil, tcp=connected and math.max(0, ms(tcp)-ms(dns)) or nil,
        tls=encrypted and math.max(0, ms(tls)-ms(tcp)) or nil, ttfb=tonumber(ttfb)>0 and ms(ttfb) or nil,
        error=rc~=0 and (curl_errors[rc] or ("检测失败（curl " .. rc .. "）")) or nil})
end
