module("luci.controller.myos_c8_status", package.seeall)
function index()
    entry({"admin", "myos_c8_status", "identity"}, call("identity")).leaf = true
    entry({"admin", "myos_c8_status", "sensors"}, call("sensors")).leaf = true
end
local function reply(data)
    local http = require("luci.http")
    http.prepare_content("application/json")
    http.write_json(data)
end
function identity()
    local source = require("myos_c8_identity")
    local mac = source.mac()
    local version_file = io.open("/rom/etc/myos-version", "r") or io.open("/etc/myos-version", "r")
    local firmware = version_file and version_file:read("*l") or ""
    if version_file then version_file:close() end
    reply({ model = "C8-688", mac = mac, device_id = source.device_id(),
        identity_available = mac ~= "", firmware = firmware,
        modem_route = { "admin", "modem", "modem" } })
end
function sensors()
    local path = "/sys/class/thermal/thermal_zone0/temp"
    local file = io.open(path, "r")
    local temperature = file and tonumber(file:read("*l")) or nil
    if file then file:close() end
    if temperature and (temperature < -20000 or temperature > 150000) then temperature = nil end
    local wifi_temps = {}
    for _, ifname in ipairs({ "ra0", "rax0" }) do
        local stats = require("luci.sys").exec("/usr/sbin/iwpriv " .. ifname .. " stat 2>/dev/null")
        local wifi_temp = tonumber(stats:match("CurrentTemperature%s*=%s*([+-]?%d+)"))
        if wifi_temp and wifi_temp >= -40 and wifi_temp <= 150 then
            wifi_temps[#wifi_temps + 1] = { ifname = ifname, milli_c = wifi_temp * 1000 }
        end
    end
    reply({ cpu_sensor = temperature and path or nil, cpu_temp = temperature,
        wifi_temps = wifi_temps, source = "cpu thermal sysfs + mt_wifi iwpriv stat", sampled_at = os.time() })
end
