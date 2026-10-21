local root = assert(arg[1])
local count = 0
local function check(v, name) assert(v, name); count = count + 1 end
local original_open = io.open
local record, uptime, exists = nil, 100, true
package.preload["luci.util"] = function() return {} end
package.preload["luci.model.uci"] = function() return { cursor = function() return {} end } end
package.preload["nixio.fs"] = function() return { readfile = function() return tostring(uptime) .. " 0" end } end
package.preload["cjson.safe"] = function() return { decode = function() return record end } end
io.open = function(path, mode)
    if path == "/tmp/myos-rssh-relay.json" then
        if not exists then return nil end
        return { read = function() return "MOCK" end, close = function() end }
    end
    return original_open(path, mode)
end
local session = assert(loadfile(root .. "/myos_nradio_app_session.lua"))()
local function reset()
    record = { service = "rssh", mac = "AABBCCDDEE12", host = "111.230.207.179", port = 28093, uniq = "new", started = 90 }
    uptime, exists = 100, true
end
reset(); check(session.ssh_relay().age_seconds == 10, "fresh authenticated relay record")
exists = false; check(session.ssh_relay() == nil, "missing relay rejected")
reset(); uptime = 211; check(session.ssh_relay() == nil, "expired relay rejected")
reset(); uptime = 80; check(session.ssh_relay() == nil, "negative age rejected")
reset(); record.service = "rweb"; check(session.ssh_relay() == nil, "web relay cannot serve SSH")
reset(); record.port = 0; check(session.ssh_relay() == nil, "invalid relay port")
reset(); record.host = "x;reboot"; check(session.ssh_relay() == nil, "invalid relay host")
reset(); record.mac = nil; check(session.ssh_relay() == nil, "identity required")
io.open = original_open
local f = assert(io.open(root .. "/myos-remote-files/usr/sbin/myos-nros-app-agent"))
local agent = f:read("*a"); f:close()
local block = assert(agent:match("(local function remote_started%(.-)\nlocal function remote_ready_output%("))
local process = "/usr/bin/ssh -p 22 -gfNTR 28093:127.0.0.1:22 debug@111.230.207.179 "
local env = setmetatable({ ok_nixiofs = true, nixiofs = { dir = function()
    local done = false; return function() if not done then done = true; return "123" end end
end }, io = { open = function() return { read = function() return process end, close = function() end } end } }, { __index = _G })
local chunk = assert(loadstring(block .. "\nreturn remote_started")); setfenv(chunk, env)
local started = chunk()
check(started("28093", "rssh", "111.230.207.179"), "exact SSH reverse target")
check(not started("28094", "rssh", "111.230.207.179"), "other port not reused")
check(not started("28093", "rssh", "134.175.214.240"), "other relay not reused")
process = process:gsub("127.0.0.1", "localhost")
check(not started("28093", "rssh", "111.230.207.179"), "ambiguous localhost target not reused")
check(agent:find('port = tonumber(port), uniq = tostring(req.uniq or ""), started = uptime', 1, true), "endpoint includes request identity and monotonic age")
check(agent:find('chmod 600', 1, true), "endpoint record permissions")
print("PASS " .. count .. " remote v2 assertions; mocked processes and files only.")
