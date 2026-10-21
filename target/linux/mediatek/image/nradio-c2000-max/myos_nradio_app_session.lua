local M = {}
local util = require "luci.util"
local uci = require("luci.model.uci").cursor()
local fs = require "nixio.fs"
local ok_json, json = pcall(require, "cjson.safe")
if not ok_json then json = require "cjson" end

local function call(method, data)
	local ok, result, code = pcall(util.ubus, "session", method, data or {})
	if not ok or (code ~= nil and code ~= 0) then return nil end
	if type(result) == "table" then return result end
	-- rpcd set/grant/destroy may succeed without a ubus reply body.
	if result == nil and (method == "set" or method == "grant" or method == "destroy") then return {} end
end

local function hex(value)
	return type(value) == "string" and #value == 32 and value:match("^[0-9a-f]+$")
end

local function matches(pattern, name)
	pattern = pattern:gsub("([%(%)%.%%%+%-%[%]%^%$])", "%%%1")
	return name:match("^" .. pattern:gsub("%*", ".*"):gsub("%?", ".") .. "$") ~= nil
end

local function permission(login, level, group)
	local entries = login[level] or {}
	if type(entries) == "string" then entries = { entries } end
	for _, entry in ipairs(entries) do
		if entry:sub(1, 1) == "!" and matches(entry:sub(2):gsub("^%s+", ""), group) then
			return false
		end
	end
	for _, entry in ipairs(entries) do
		if entry:sub(1, 1) ~= "!" and matches(entry, group) then return true end
	end
	return level == "read" and permission(login, "write", group) or false
end

-- Build the same scoped grants as rpcd's configured root login.
local function root_grants()
	local login
	uci:foreach("rpcd", "login", function(section)
		if not login and section.username == "root" then login = section end
	end)
	if not login then return nil end
	local grants, groups = {}, 0
	local function add(scope, object, method)
		if type(scope) ~= "string" or type(object) ~= "string" or type(method) ~= "string" then return end
		grants[scope] = grants[scope] or {}
		grants[scope][#grants[scope] + 1] = { object, method }
	end
	for path in fs.glob("/usr/share/rpcd/acl.d/*.json") do
		local f = io.open(path, "r")
		if f then
			local raw = f:read("*a")
			f:close()
			local ok, acl = pcall(json.decode, raw)
			if ok and type(acl) == "table" then
				for group, rights in pairs(acl) do
					if type(rights) == "table" then
						for _, level in ipairs({ "read", "write" }) do
							local scopes = rights[level]
							if type(scopes) == "table" and permission(login, level, group) then
								groups = groups + 1
								add("access-group", group, level)
								for scope, objects in pairs(scopes) do
									if type(objects) == "table" then
										for object, methods in pairs(objects) do
											if type(object) == "number" then
												add(scope, methods, level)
											elseif type(methods) == "table" then
												for _, method in ipairs(methods) do add(scope, object, method) end
											end
										end
									end
								end
							end
						end
					end
				end
			end
		end
	end
	return groups > 0 and grants or nil
end

function M.destroy(sid)
	if hex(sid) then call("destroy", { ubus_rpc_session = sid }) end
end

-- Caller must first verify the encrypted APP signature, timestamp and replay guard.
function M.create(csrf)
	if not hex(csrf) then return "" end
	local grants = root_grants()
	if not grants then return "" end
	local session = call("create", { timeout = 3600 })
	local sid = session and session.ubus_rpc_session
	if not hex(sid) or sid == string.rep("0", 32) then return "" end
	if not call("set", { ubus_rpc_session = sid,
		values = { username = "root", token = csrf, myos_app = true } }) then
		M.destroy(sid)
		return ""
	end
	for scope, objects in pairs(grants) do
		if not call("grant", { ubus_rpc_session = sid, scope = scope, objects = objects }) then
			M.destroy(sid)
			return ""
		end
	end
	return sid
end

function M.valid(sid)
	if not hex(sid) then return false end
	local session = call("get", { ubus_rpc_session = sid })
	local values = session and session.values
	return type(values) == "table" and values.username == "root" and
		values.myos_app == true and not not hex(values.token)
end

function M.cookie(sid)
	local name = os.getenv("HTTPS") == "on" and "sysauth_https" or "sysauth_http"
	local secure = name == "sysauth_https" and "; Secure" or ""
	return name .. "=" .. sid .. "; Max-Age=3600; HttpOnly; SameSite=Lax; Path=/cgi-bin/luci" .. secure
end

local function ssh_running()
	local ok, nixio = pcall(require, "nixio")
	if not ok then return false end
	local socket = nixio.socket("inet", "stream")
	if not socket then return false end
	socket:setopt("socket", "rcvtimeo", 1)
	local connected = socket:connect("127.0.0.1", 22)
	local banner = connected and socket:recv(256) or ""
	socket:close()
	return type(banner) == "string" and banner:match("^SSH%-2%.0%-") ~= nil
end

function M.start_ssh()
	if ssh_running() then return true end
	local init = fs.access("/etc/init.d/myos-nros-ssh", "x") and "myos-nros-ssh" or "dropbear"
	local rc = os.execute("/etc/init.d/" .. init .. " start >/dev/null 2>&1")
	if rc ~= true and rc ~= 0 then return false end
	for _ = 1, 3 do
		if ssh_running() then return true end
		os.execute("sleep 1")
	end
	return ssh_running()
end

-- Read only after APP authentication. Never infer a relay from an assigned port.
function M.ssh_relay()
	local f = io.open("/tmp/myos-rssh-relay.json", "r")
	if not f then return nil end
	local raw = f:read(8192)
	f:close()
	local ok, relay = pcall(json.decode, raw or "")
	if not ok or type(relay) ~= "table" or relay.service ~= "rssh" then return nil end
	local uptime = tonumber((fs.readfile("/proc/uptime") or ""):match("^[%d.]+"))
	local age = uptime and tonumber(relay.started) and uptime - tonumber(relay.started)
	if not age or age < 0 or age > 120 then return nil end
	local port = tonumber(relay.port)
	if not port or port < 1 or port > 65535 or type(relay.host) ~= "string" or
	   not relay.host:match("^[%w.%-]+$") or type(relay.mac) ~= "string" then return nil end
	relay.age_seconds = math.floor(age)
	return relay
end

-- Only the authenticated service API calls this; no arbitrary shell input.
function M.ssh_service(req, device_code)
	if req.cmd ~= "service" or req.name ~= "dropbear" or
	   type(device_code) ~= "string" or device_code == "" or req.device_code ~= device_code then
		return { code = "1", result = "invalid SSH service request" }
	end
	if req.enabled ~= nil and req.enabled ~= 1 and req.enabled ~= "1" then
		return { code = "1", result = "only SSH enable is supported" }
	end
	if req.enabled ~= nil and not M.start_ssh() then
		return { code = "1", enabled = 0, result = "SSH service failed to start" }
	end
	return { code = "0", name = "dropbear", enabled = ssh_running() and 1 or 0,
		remote_access_version = 2, relay = M.ssh_relay() }
end

return M
