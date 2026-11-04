-- Copyright 2022 xiaobao <xiaobao@linkease.com>
-- Licensed to the public under the MIT License

local http = require "luci.http"
local nixio = require "nixio"
local ltn12 = require "luci.ltn12"
local table = require "table"
local util = require "luci.util"

module("luci.controller.istore_backend", package.seeall)

local BLOCKSIZE = 2048
local ISTOREOS_PORT = 3038

function index()
    entry({"istore"}, call("istore_backend")).leaf=true
end

local function sink_socket(sock, io_err) 
  if sock then 
    return function(chunk, err) 
      if not chunk then 
        return 1 
      else 
        return sock:send(chunk)
      end 
    end 
  else 
    return ltn12.sink.error(io_err or "unable to send socket") 
  end
end

local function sink_table(target)
  return function(chunk, err)
    if chunk then
      target[#target+1] = chunk
    end
    return 1
  end
end

local function session_retrieve(sid, allowed_users)
  local sdat = util.ubus("session", "get", { ubus_rpc_session = sid })
  if type(sdat) == "table" and
      type(sdat.values) == "table" and
      type(sdat.values.token) == "string" and
      (not allowed_users or
      util.contains(allowed_users, sdat.values.username))
  then
      return sid, sdat.values
  end
  return nil, nil
end

local function get_session()
  local sid
  local key
  local sdat
  for _, key in ipairs({"sysauth_https", "sysauth_http", "sysauth"}) do
    sid = http.getcookie(key)
    if sid then
      sid, sdat = session_retrieve(sid, nil)
      if sid and sdat then
        return sid, sdat
      end
    end
  end
  return nil, nil
end

local function chunksource(sock, buffer)
	buffer = buffer or ""
	return function()
		local output
		local _, endp, count = buffer:find("^([0-9a-fA-F]+);?.-\r\n")
		while not count and #buffer <= 1024 do
			local newblock, code = sock:recv(1024 - #buffer)
			if not newblock then
				return nil, code
			end
			buffer = buffer .. newblock  
			_, endp, count = buffer:find("^([0-9a-fA-F]+);?.-\r\n")
		end
		count = tonumber(count, 16)
		if not count then
			return nil, -1, "invalid encoding"
		elseif count == 0 then
			return nil
		elseif count + 2 <= #buffer - endp then
			output = buffer:sub(endp+1, endp+count)
			buffer = buffer:sub(endp+count+3)
			return output
		else
			output = buffer:sub(endp+1, endp+count)
			buffer = ""
			if count - #output > 0 then
				local remain, code = sock:recvall(count-#output)
				if not remain then
					return nil, code
				end
				output = output .. remain
				count, code = sock:recvall(2)
			else
				count, code = sock:recvall(count+2-#buffer+endp)
			end
			if not count then
				return nil, code
			end
			return output
		end
	end
end

local function read_cpu_temperature()
  local path, file, raw, value

  for i = 0, 9 do
    path = string.format("/sys/class/thermal/thermal_zone%d/temp", i)
    file = io.open(path, "r")
    if file then
      raw = file:read("*l")
      file:close()
      value = tonumber(raw)
      if value and value > 0 then
        if value > 1000 then
          value = value / 1000
        end
        return math.floor(value + 0.5)
      end
    end
  end

  return nil
end

local function add_system_status_fields(body)
  local jsonc = require "luci.jsonc"
  local ok, data = pcall(jsonc.parse, body)

  if not ok or type(data) ~= "table" or type(data.result) ~= "table" then
    return body
  end

  local cpu_temperature = read_cpu_temperature()
  if cpu_temperature then
    data.result.cpuTemperature = cpu_temperature
  end

  return jsonc.stringify(data)
end

local function add_network_status_fallback(body)
  local jsonc = require "luci.jsonc"
  local ok, data = pcall(jsonc.parse, body)

  if not ok or type(data) ~= "table" or type(data.result) ~= "table" then
    return body
  end

  local result = data.result
  if result.networkInfo == "netDetecting" or result.networkInfo == "softSourceFailed" then
    local has_ipv4 = type(result.ipv4addr) == "string" and result.ipv4addr ~= "" and result.ipv4addr ~= "0.0.0.0"
    local has_ipv6 = type(result.ipv6addr) == "string" and result.ipv6addr ~= ""
    local has_gateway = type(result.gateway) == "string" and result.gateway ~= ""
    local has_dns = type(result.dnsList) == "table" and #result.dnsList > 0

    if (has_ipv4 or has_ipv6) and (has_gateway or has_dns) then
      result.networkInfo = "netSuccess"
    end
  end

  return jsonc.stringify(data)
end

local function human_bytes(bytes)
  local units = {"B", "K", "M", "G", "T"}
  local value = tonumber(bytes) or 0
  local index = 1

  while value >= 1024 and index < #units do
    value = value / 1024
    index = index + 1
  end

  if value >= 10 or index == 1 then
    return string.format("%d%s", math.floor(value + 0.5), units[index])
  end

  return string.format("%.1f%s", value, units[index])
end

local function basename(path)
  return tostring(path or ""):match("([^/]+)$") or tostring(path or "")
end

local function add_mount_disk(disks, source, mountpoint, fstype, options)
  local stat = nixio.fs.statvfs(mountpoint)
  if type(stat) ~= "table" or not stat.blocks then
    return
  end

  local block_size = stat.frsize or stat.bsize or BLOCKSIZE
  local total = (tonumber(stat.blocks) or 0) * block_size
  local free = (tonumber(stat.bfree) or 0) * block_size
  local used = total - free
  local usage = 0

  if total > 0 then
    usage = math.floor(used * 100 / total + 0.5)
  end

  disks[#disks+1] = {
    name = mountpoint == "/" and "Rootfs" or basename(source),
    size = human_bytes(total),
    used = human_bytes(used),
    usage = usage,
    venderModel = "",
    isSystemRoot = mountpoint == "/",
    childrens = {{
      name = basename(source),
      path = source,
      filesystem = fstype or "unknown",
      mountPoint = mountpoint,
      isReadOnly = options and options:find("ro", 1, true) ~= nil or false,
    }},
  }
end

local function build_disk_status_fallback()
  local disks = {}
  local file = io.open("/proc/mounts", "r")

  if file then
    for line in file:lines() do
      local source, mountpoint, fstype, options = line:match("^(%S+)%s+(%S+)%s+(%S+)%s+(%S+)")
      if source and mountpoint and fstype then
        if mountpoint == "/" or (source:match("^/dev/") and mountpoint:match("^/mnt/")) then
          add_mount_disk(disks, source, mountpoint, fstype, options)
        end
      end
    end
    file:close()
  end

  return {
    success = 0,
    result = {
      disks = disks,
    },
  }
end

local function add_disk_status_fallback(body)
  local jsonc = require "luci.jsonc"
  local ok, data = pcall(jsonc.parse, body)

  if ok and type(data) == "table" and type(data.result) == "table" and type(data.result.disks) == "table" then
    return body
  end

  return jsonc.stringify(build_disk_status_fallback())
end

local function read_first_line(path)
  local file = io.open(path, "r")
  local line

  if file then
    line = file:read("*l")
    file:close()
  end

  return line
end

local function readlink_name(path)
  local ok, target

  if not nixio.fs or not nixio.fs.readlink then
    return ""
  end

  ok, target = pcall(nixio.fs.readlink, path)
  if ok and target then
    return basename(target)
  end

  return ""
end

local function append_unique(list, value)
  if not value or value == "" then
    return
  end

  for _, existing in ipairs(list) do
    if existing == value then
      return
    end
  end

  list[#list+1] = value
end

local function list_value(value)
  local values = {}

  if type(value) == "table" then
    for _, item in ipairs(value) do
      append_unique(values, item)
    end
  elseif type(value) == "string" then
    for item in value:gmatch("%S+") do
      append_unique(values, item)
    end
  end

  return values
end

local function build_network_port_map()
  local port_map = {}
  local bridge_ports = {}
  local ok, uci_mod = pcall(require, "luci.model.uci")

  if not ok or type(uci_mod) ~= "table" then
    return port_map
  end

  local cursor = uci_mod.cursor()

  cursor:foreach("network", "device", function(section)
    if section.type == "bridge" then
      local bridge_name = section.name or section[".name"]
      if bridge_name and bridge_name ~= "" then
        bridge_ports[bridge_name] = list_value(section.ports or section.ifname)
      end
    end
  end)

  cursor:foreach("network", "interface", function(section)
    local interface = section[".name"]
    local devices = list_value(section.device or section.ifname)

    if not interface or interface == "" then
      return
    end

    for _, device in ipairs(devices) do
      port_map[device] = port_map[device] or {}
      append_unique(port_map[device], interface)

      if bridge_ports[device] then
        for _, port in ipairs(bridge_ports[device]) do
          port_map[port] = port_map[port] or {}
          append_unique(port_map[port], interface)
        end
      end
    end
  end)

  return port_map
end

local function is_network_port(name)
  local lower = tostring(name or ""):lower()

  if lower == "" or lower == "lo" then
    return false
  end

  if lower:match("^br%-") or lower:match("^docker") or lower:match("^veth") or
      lower:match("^ifb") or lower:match("^gre") or lower:match("^sit") or
      lower:match("^tun") or lower:match("^tap") or lower:match("^wlan") or
      lower:match("^ra%d") or lower:match("^rai%d") or lower:match("^phy") then
    return false
  end

  return lower == "wan" or lower:match("^lan%d+$") ~= nil or
      lower:match("^eth%d+$") ~= nil or lower:match("^blue%d+$") ~= nil or
      nixio.fs.access("/sys/class/net/" .. name .. "/speed")
end

local function link_state(name)
  local carrier = read_first_line("/sys/class/net/" .. name .. "/carrier")
  local operstate

  if carrier == "1" then
    return "UP"
  elseif carrier == "0" then
    return "DOWN"
  end

  operstate = read_first_line("/sys/class/net/" .. name .. "/operstate")
  if operstate == "up" then
    return "UP"
  end

  return "DOWN"
end

local function format_link_speed(raw)
  local value = tonumber(raw)

  if not value or value <= 0 then
    return ""
  end

  if value >= 1000 then
    local gbps = value / 1000
    if gbps == math.floor(gbps) then
      return string.format("%dG", math.floor(gbps))
    end
    return string.format("%.1fG", gbps)
  end

  return string.format("%dM", value)
end

local function port_rank(name)
  local lower = tostring(name or ""):lower()
  local number = tonumber(lower:match("(%d+)$")) or 0

  if lower:match("^lan%d+$") then
    return 100 + number
  elseif lower == "wan" then
    return 200
  elseif lower:match("^blue%d+$") then
    return 300 + number
  elseif lower:match("^eth%d+$") then
    return 400 + number
  end

  return 900
end

local function build_port_status_fallback()
  local ports = {}
  local port_map = build_network_port_map()

  if nixio.fs and nixio.fs.dir then
    for name in nixio.fs.dir("/sys/class/net") do
      if is_network_port(name) then
        local master = readlink_name("/sys/class/net/" .. name .. "/master")
        local interfaces = {}

        if port_map[name] then
          for _, interface in ipairs(port_map[name]) do
            append_unique(interfaces, interface)
          end
        end

        if master ~= "" and port_map[master] then
          for _, interface in ipairs(port_map[master]) do
            append_unique(interfaces, interface)
          end
        end

        ports[#ports+1] = {
          name = name,
          macAddress = read_first_line("/sys/class/net/" .. name .. "/address") or "",
          linkSpeed = format_link_speed(read_first_line("/sys/class/net/" .. name .. "/speed")),
          linkState = link_state(name),
          rx_packets = read_first_line("/sys/class/net/" .. name .. "/statistics/rx_packets") or "",
          tx_packets = read_first_line("/sys/class/net/" .. name .. "/statistics/tx_packets") or "",
          interfaceNames = #interfaces > 0 and interfaces or nil,
          master = master,
          duplex = read_first_line("/sys/class/net/" .. name .. "/duplex") or "",
        }
      end
    end
  end

  table.sort(ports, function(left, right)
    local left_rank = port_rank(left.name)
    local right_rank = port_rank(right.name)

    if left_rank == right_rank then
      return left.name < right.name
    end

    return left_rank < right_rank
  end)

  return {
    success = 0,
    result = {
      ports = ports,
    },
  }
end

local function add_port_status_fallback(body)
  local jsonc = require "luci.jsonc"
  local ok, data = pcall(jsonc.parse, body)

  if ok and type(data) == "table" and type(data.result) == "table" and
      type(data.result.ports) == "table" and #data.result.ports > 0 then
    return body, false
  end

  return jsonc.stringify(build_port_status_fallback()), true
end

function istore_backend() 
  local sock = nixio.connect("127.0.0.1", ISTOREOS_PORT) 
  if not sock then
    http.status(500, "connect failed")
    return
  end
  local input = {}
  input[#input+1] = http.getenv("REQUEST_METHOD") .. " " .. http.getenv("REQUEST_URI") .. " HTTP/1.1"
  local req = http.context.request
  local start = "HTTP_"
  local start_len = string.len(start)
  local ctype = http.getenv("CONTENT_TYPE")
  if ctype then 
    input[#input+1] = "Content-Type: " .. ctype 
  end
  for k, v in pairs(req.message.env) do
    if string.sub(k, 1, start_len) == start and not string.find(k, "FORWARDED") then 
      input[#input+1] = string.sub(k, start_len+1, string.len(k)) .. ": " .. v
    end
  end
  local sid, sdat = get_session()
  if sdat ~= nil then
    input[#input+1] = "X-Forwarded-Sid: " .. sid
    input[#input+1] = "X-Forwarded-Token: " .. sdat.token
  end
  -- input[#input+1] = "X-Forwarded-For: " .. http.getenv("REMOTE_HOST") ..":".. http.getenv("REMOTE_PORT")
  local num = tonumber(http.getenv("CONTENT_LENGTH")) or 0
  input[#input+1] = "Content-Length: " .. tostring(num)
  input[#input+1] = "\r\n"
  local source = ltn12.source.cat(ltn12.source.string(table.concat(input, "\r\n")), http.source())
  local ret = ltn12.pump.all(source, sink_socket(sock, "write sock error")) 
  if ret ~= 1 then
    sock:close()
    http.status(500, "proxy error")
    return
  end

  local linesrc = sock:linesource()
  local line, code, error = linesrc()
  if not line then
    sock:close()
    http.status(500, "response parse failed")
    return
  end

  local protocol, status, msg = line:match("^([%w./]+) ([0-9]+) (.*)")
  if not protocol then
    sock:close()
    http.status(500, "response protocol error")
    return
  end
  num = tonumber(status) or 0
  http.status(num, msg)

  local chunked = 0
  line = linesrc()
  while line and line ~= "" do
    local key, val = line:match("^([%w-]+)%s?:%s?(.*)")
    if key and key ~= "Status" then
      if key == "Transfer-Encoding" and val == "chunked" then
        chunked = 1
      end
      if key ~= "Connection" and key ~= "Transfer-Encoding" and key ~= "Content-Length" then 
        http.header(key, val)
      end
    end
    line = linesrc()
  end
  if not line then
    sock:close()
    http.status(500, "parse header failed")
    return
  end

  local body_buffer = linesrc(true)
  local body_chunks = {}
  if chunked == 1 then
    ltn12.pump.all(chunksource(sock, body_buffer), sink_table(body_chunks))
  else
    local body_source = ltn12.source.cat(ltn12.source.string(body_buffer), sock:blocksource())
    ltn12.pump.all(body_source, sink_table(body_chunks))
  end

  local body = table.concat(body_chunks)
  local request_uri = http.getenv("REQUEST_URI") or ""
  if request_uri:find("/system/status/", 1, true) then
    body = add_system_status_fields(body)
  elseif request_uri:find("/u/network/status/", 1, true) then
    body = add_network_status_fallback(body)
  elseif request_uri:find("/nas/disk/status/", 1, true) then
    body = add_disk_status_fallback(body)
  elseif request_uri:find("/network/port/list/", 1, true) then
    local used_fallback
    body, used_fallback = add_port_status_fallback(body)
    if used_fallback then
      http.status(200, "OK")
    end
  end

  http.write(body)

  sock:close()
end
