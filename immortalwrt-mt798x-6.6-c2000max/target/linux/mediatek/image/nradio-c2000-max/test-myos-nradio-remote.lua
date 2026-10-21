local root = assert(arg[1], "asset directory required")
local count = 0
local function check(value, message)
	assert(value, message)
	count = count + 1
end
local function read(path)
	local f = assert(io.open(root .. "/" .. path, "r"))
	local value = f:read("*a")
	f:close()
	return value
end
local sources = {}
for _, name in ipairs({ "myos_nradio_app_session.lua", "myos-nradio-app.lua",
	"myos-nradio-app-fast.lua", "myos-nros-cloud-cbi.lua",
	"myos-remote-files/usr/sbin/myos-nros-app-agent" }) do
	check(loadfile(root .. "/" .. name), "Lua syntax: " .. name)
	sources[name] = read(name)
end
local original_open, original_execute, original_getenv = io.open, os.execute, os.getenv
local env, sessions, calls, fail, serial, running, helper = {}, {}, {}, nil, 0, false, true
local root_login = { username = "root", read = { "*", "!private" }, write = { "luci-*" } }
local acl = {
	["luci-base"] = {
		read = { ubus = { system = { "board" } }, uci = { "network" } },
		write = { ubus = { uci = { "*" } }, file = { ["/etc/config/*"] = { "read", "write" } } }
	},
	private = { read = { ubus = { secret = { "*" } } } }
}
local cursor = {}
function cursor:foreach(_, _, callback) if root_login then callback(root_login) end end
package.preload["luci.model.uci"] = function() return { cursor = function() return cursor end } end
package.preload["nixio.fs"] = function()
	return {
		glob = function()
			local done = false
			return function() if not done then done = true; return "/mock/acl.json" end end
		end,
		access = function() return helper end
	}
end
package.preload["cjson.safe"] = function() return { decode = function(raw) if raw == "ACL" then return acl end end } end
package.preload["luci.util"] = function()
	return { ubus = function(object, method, data)
		check(object == "session", "native rpcd object")
		calls[#calls + 1] = { method = method, data = data }
		if fail == method then return nil, 7 end
		local sid = data.ubus_rpc_session
		if method == "create" then
			serial = serial + 1
			sid = string.format("%032x", serial)
			sessions[sid] = { grants = {} }
			return { ubus_rpc_session = sid }
		elseif method == "destroy" then sessions[sid] = nil; return
		elseif not sessions[sid] then return nil, 4
		elseif method == "set" then sessions[sid].values = data.values; return
		elseif method == "grant" then sessions[sid].grants[data.scope] = data.objects; return
		elseif method == "get" then return { values = sessions[sid].values }
		end
	end }
end
package.preload["nixio"] = function()
	return { socket = function()
		return { setopt = function() end, connect = function(_, host, port)
			check(host == "127.0.0.1" and port == 22, "SSH probe stays on loopback")
			return running
		end, recv = function() return "SSH-2.0-dropbear\r\n" end, close = function() end }
	end }
end
io.open = function(path, mode)
	if path == "/mock/acl.json" then return { read = function() return "ACL" end, close = function() end } end
	return original_open(path, mode)
end
os.getenv = function(name) return env[name] end
local executed = {}
os.execute = function(command)
	executed[#executed + 1] = command
	if command:match("^/etc/init.d/") then running = true end
	return 0
end
local session = assert(loadfile(root .. "/myos_nradio_app_session.lua"))()
local csrf = string.rep("a", 32)
local sid = session.create(csrf)
check(#sid == 32 and sid ~= csrf, "APP token is a real rpcd session id")
check(session.valid(sid), "native session validates")
check(sessions[sid].values.username == "root" and sessions[sid].values.token == csrf, "LuCI user and CSRF values")
local groups = sessions[sid].grants["access-group"]
check(groups and #groups == 2 and groups[1][1] == "luci-base" and groups[2][1] == "luci-base", "configured ACL groups exposed to LuCI")
check(sessions[sid].grants.uci[1][1] == "network" and sessions[sid].grants.uci[1][2] == "read", "array ACL scopes converted")
check(sessions[sid].grants.file[1][1] == "/etc/config/*", "object ACL scopes converted")
sessions[sid].values.token = false
check(not session.valid(sid), "invalid CSRF cannot authorize")
sessions[sid].values.token = csrf
session.destroy(sid)
check(not session.valid(sid), "destroyed rpcd session rejected")
for _, method in ipairs({ "create", "set", "grant" }) do
	fail = method
	check(session.create(csrf) == "", "rpcd " .. method .. " failure closes authentication")
	fail = nil
end
root_login = nil
check(session.create(csrf) == "", "missing configured root ACL fails closed")
root_login = { username = "root", read = { "*" }, write = { "*" } }
check(session.create("cloud-token") == "", "malformed APP proof cannot create session")
check(session.cookie(csrf):match("^sysauth_http="), "HTTP native LuCI cookie")
env.HTTPS = "on"
check(session.cookie(csrf):match("^sysauth_https=") and session.cookie(csrf):match("; Secure$"), "HTTPS native secure cookie")
env.HTTPS = nil

running = false
local req = { cmd = "service", name = "dropbear", device_code = "router" }
check(session.ssh_service(req, "router").enabled == 0, "SSH status query")
req.enabled = 1
local status = session.ssh_service(req, "router")
check(status.code == "0" and status.enabled == 1, "SSH enable verifies listener")
check(executed[#executed] == "/etc/init.d/myos-nros-ssh start >/dev/null 2>&1", "SSH starts loopback service")
local before = #executed
check(session.ssh_service({ cmd = "service", name = "dropbear;reboot", device_code = "router", enabled = 1 }, "router").code == "1", "arbitrary service rejected")
check(session.ssh_service({ cmd = "service", name = "dropbear", device_code = "foreign", enabled = 1 }, "router").code == "1", "foreign device rejected")
check(session.ssh_service({ cmd = "service", name = "dropbear", device_code = "router", enabled = 0 }, "router").code == "1", "unsupported SSH action rejected")
check(#executed == before, "rejected service requests have no effects")

local agent = sources["myos-remote-files/usr/sbin/myos-nros-app-agent"]
local function block(source, name, next_name)
	return assert(source:match("(local function " .. name .. "%(.-)\nlocal function " .. next_name .. "%("), name)
end
local function load_env(program, environment)
	local chunk = assert(loadstring(program))
	setfenv(chunk, setmetatable(environment, { __index = _G }))
	return chunk()
end
local trim = function(v) return tostring(v or ""):gsub("^%s+", ""):gsub("%s+$", "") end
local parse = load_env(block(agent, "parse_remote_command", "read_all") .. "\nreturn parse_remote_command", { trim = trim })
local command = "wget -q --ca-certificate=/etc/ssl/certs/ca-certificates.crt -O- nradio-cloud.oss-cn-beijing.aliyuncs.com/cloud-ctrl-new.sh | sh -s -- -p 37272 -s rweb -r 134.175.214.240"
local port, relay, service = parse(command)
check(port == "37272" and relay == "134.175.214.240" and service == "rweb", "official rweb request accepted")
local ssh_command = command:gsub("rweb", "rssh")
port, relay, service = parse(ssh_command)
check(port == "37272" and service == "rssh", "official rssh request accepted")
for _, bad in ipairs({ command .. "; reboot", command:gsub("rweb", "shell"),
	command:gsub("37272", "0"), command:gsub("37272", "65536"),
	command:gsub("nradio%-cloud.oss%-cn%-beijing.aliyuncs.com", "evil.example") }) do
	check(parse(bad) == nil, "unsafe or unsupported tunnel command rejected")
end
local flags = { enabled = "1", remote_web_enabled = "1", remote_ssh_enabled = "0" }
local gate_uci = { load = function() end, get = function(_, _, _, option) return flags[option] end }
local enabled = load_env(block(agent, "remote_enabled", "write_shared_summary") .. "\nreturn remote_enabled", { uci = gate_uci })
check(enabled("rweb") and not enabled("rssh"), "independent remote access gates")
flags.enabled = "0"
check(not enabled("rweb"), "master cloud switch closes tunnels")
flags.enabled, flags.remote_ssh_enabled = "1", "1"
check(not enabled("shell"), "unknown service gate closed")
local tunnel_commands, started, ssh_ok, key_exists, ssh_starts = {}, false, true, true, 0
local run_tunnel = load_env(block(agent, "run_remote_command", "command_json") .. "\nreturn run_remote_command", {
	app_session = { start_ssh = function() ssh_starts = ssh_starts + 1; return ssh_ok end },
	remote_started = function() return started end,
	remote_ready_output = function(p, kind) return kind .. "\nReady to RSSH\nPort: " .. p end,
	shell_quote = function(v) return "'" .. v .. "'" end,
	idle_sleep = function() end,
	read_all = function() return "\n__MYOS_RWEB_RC__0\n" end,
	io = { open = function()
		if key_exists then return { close = function() end } end
	end },
	os = { time = function() return 100 end, remove = function() end, execute = function(cmd)
		tunnel_commands[#tunnel_commands + 1] = cmd
		started = true
	end },
	util = { exec = function() end }
})
local rc, output = run_tunnel("portable-download", "37272", "134.175.214.240", "rweb")
check(rc == "0" and output:match("^rweb\n"), "direct web callback")
check(tunnel_commands[1]:find("-p 23 -gfNTR '37272:127.0.0.1:80'", 1, true), "web relay and destination ports")
check(ssh_starts == 0, "web tunnel does not start SSH server")
started = false
rc, output = run_tunnel("portable-download", "37273", "134.175.214.240", "rssh")
check(rc == "0" and output:match("^rssh\n"), "direct SSH callback")
check(tunnel_commands[2]:find("-p 22 -gfNTR '37273:127.0.0.1:22'", 1, true), "SSH relay and destination ports")
check(ssh_starts == 1, "SSH tunnel verifies local service")
started, key_exists = false, false
run_tunnel("portable-download", "37274", "134.175.214.240", "rweb")
check(tunnel_commands[3]:find("portable-download", 1, true), "missing key uses download fallback")
started, ssh_ok = false, false
rc = run_tunnel("portable-download", "37275", "134.175.214.240", "rssh")
check(rc == "1" and #tunnel_commands == 3, "failed SSH listener prevents tunnel launch")

local receipt, requested
local handle = load_env(block(agent, "handle_command", "info_report") .. "\nreturn handle_command", {
	ID = "AABBCCDDEEFF", uci = gate_uci, trim = trim,
	remote_started = function() return false end,
	os = { remove = function() end },
	parse_bridge_update = function() end, parse_remote_command = parse, remote_enabled = enabled,
	shell_quote = function(v) return "'" .. v .. "'" end,
	run_remote_command = function(safe, p, host, kind)
		requested = { safe = safe, port = p, host = host, kind = kind }
		return "0", kind .. "\nReady to RSSH\nPort: " .. p
	end,
	reply = function(event, value) receipt = { event = event, value = value } end,
	util = { exec = function() return "" end }
})
handle({ cmd = command, id = "AABBCCDDEEFF", uniq = "request-1" }, false)
check(requested.kind == "rweb" and requested.safe:find("-T 15", 1, true), "portable wget fallback")
check(not requested.safe:find("--tries", 1, true), "uclient incompatible option removed")
check(receipt.value.uniq == "request-1" and receipt.value["return"] == "0", "callback preserves correlation")
handle({ cmd = ssh_command }, false)
check(requested.kind == "rssh" and requested.safe:find("-s rssh", 1, true), "SSH fallback receives service")
requested = nil
handle({ cmd = command }, true)
check(requested == nil and receipt.value["return"] == "126", "broadcast cannot open tunnel")
flags.remote_web_enabled = "0"
handle({ cmd = command }, false)
check(requested == nil and receipt.value["return"] == "126", "disabled web cannot open tunnel")

for _, name in ipairs({ "myos-nradio-app.lua", "myos-nradio-app-fast.lua" }) do
	local source = sources[name]
	local files, stamp = {}, 100
	local auth_env = {
		app_session = session, auth_dir = "/mock/auth",
		monotonic_time = function() return stamp end,
		shell_quote = function(v) return "'" .. v .. "'" end,
		read1 = function(path) return files[path] or "" end,
		write_session = function(path, expires) files[path] = tostring(expires); return true end,
		trim = trim,
		os = { execute = function() return 0 end, remove = function(path) files[path] = nil end,
			getenv = function(key) return env[key] end },
		io = { open = function(path)
			check(path == "/dev/urandom", "auth uses random CSRF")
			return { read = function() return string.rep("z", 16) end, close = function() end }
		end, popen = function() return nil end }
	}
	local issue = load_env(block(source, "issue_auth_token", "authorized") .. "\nreturn issue_auth_token", auth_env)
	local token = issue()
	check(#token == 32 and session.valid(token), name .. " issues native session")
	local next_fn = name == "myos-nradio-app.lua" and "local_enabled" or "local_enabled"
	local authorize = load_env(block(source, "authorized", next_fn) .. "\nreturn authorized", auth_env)
	for _, cookie in ipairs({ "sysauth", "sysauth_http", "sysauth_https" }) do
		env.HTTP_COOKIE = cookie .. "=" .. token
		check(authorize({}), name .. " accepts " .. cookie)
	end
	env.HTTP_COOKIE, env.HTTP_X_ACCESS_TOKEN = "", csrf
	check(not authorize({}), name .. " rejects cloud token without APP auth")
	env.HTTP_X_ACCESS_TOKEN = nil
	session.destroy(token)
	check(not authorize({ token = token }), name .. " rejects expired native session")
	check(source:find("app_session.ssh_service(req, device_code())", 1, true), name .. " wires SSH service API")
	check(source:find("app_session.cookie(token)", 1, true), name .. " emits LuCI cookie")
end
local packaging = read("MYOS-nros-app-patch.sh")
check(packaging:find('myos_nradio_app_session.lua', 1, true), "session helper installed")
check(packaging:find('myos-nros-ssh', 1, true) and packaging:find('30-myos-nros-remote-access', 1, true), "loopback service and migration installed")
check(read("myos-remote-files/etc/uci-defaults/29-myos-nros-app"):find("remote_web_enabled='1'", 1, true), "first-boot web enabled")
check(read("myos-remote-files/etc/init.d/myos-nros-ssh"):find("127.0.0.1:22", 1, true), "tunnel SSH binds loopback")

io.open, os.execute, os.getenv = original_open, original_execute, original_getenv
for _, name in ipairs({ "MYOS-nros-app-patch.sh", "myos-remote-files/usr/sbin/myos-nros-cloudctl",
	"myos-remote-files/etc/uci-defaults/29-myos-nros-app",
	"myos-remote-files/etc/uci-defaults/30-myos-nros-remote-access",
	"myos-remote-files/etc/init.d/myos-nros-ssh" }) do
	local rc = os.execute("busybox ash -n '" .. root .. "/" .. name .. "'")
	check(rc == 0 or rc == true, "ash syntax: " .. name)
end
print("PASS: " .. count .. " remote access checks")
