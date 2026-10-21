-- Copyright 2022 xiaobao <xiaobao@linkease.com>
-- Licensed to the public under the MIT License

local http = require "luci.http"
local nixio = require "nixio"
local fs = require "nixio.fs"
local ltn12 = require "luci.ltn12"
local table = require "table"
local util = require "luci.util"
local jsonc = require "luci.jsonc"

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

local function human_size(bytes)
  local n = tonumber(bytes) or 0
  local units = {"B", "K", "M", "G", "T"}
  local idx = 1
  while n >= 1024 and idx < #units do
    n = n / 1024
    idx = idx + 1
  end
  if n >= 10 or idx == 1 then
    return string.format("%.0f%s", n, units[idx])
  end
  return string.format("%.1f%s", n, units[idx])
end

local function df_map()
  local map = {}
  for line in util.exec("df -P 2>/dev/null"):gmatch("[^\r\n]+") do
    local _, blocks, used, _, pct, mount = line:match("^(%S+)%s+(%d+)%s+(%d+)%s+(%d+)%s+(%d+)%%%s+(.+)$")
    if mount then
      map[mount] = {
        total = (tonumber(blocks) or 0) * 1024,
        used = (tonumber(used) or 0) * 1024,
        usage = tonumber(pct) or 0
      }
    end
  end
  return map
end

local function is_system_mount(mount)
  return mount == "/" or mount == "/rom" or mount == "/overlay"
end

local function part_status(dev, dfs)
  local mount = type(dev.mountpoint) == "string" and dev.mountpoint or nil
  local stat = mount and dfs[mount] or nil
  local fs = type(dev.fstype) == "string" and dev.fstype or "No FileSystem"
  return {
    name = dev.name or "",
    path = dev.path or (dev.name and ("/dev/" .. dev.name) or ""),
    filesystem = fs,
    mountPoint = mount,
    uuid = type(dev.uuid) == "string" and dev.uuid or "",
    total = human_size(stat and stat.total or dev.size),
    used = human_size(stat and stat.used or 0),
    usage = stat and stat.usage or 0,
    isReadOnly = dev.ro == true,
    isSystemRoot = is_system_mount(mount)
  }
end

local function disk_status()
  local _, sdat = get_session()
  http.prepare_content("application/json")
  if not sdat then
    http.write(jsonc.stringify({success = -1001, error = "Forbidden"}))
    return
  end

  local raw = util.exec("lsblk -J -b -o NAME,PATH,SIZE,TYPE,FSTYPE,MOUNTPOINT,UUID,RO 2>/dev/null")
  local parsed = jsonc.parse(raw) or {}
  local dfs = df_map()
  local rootstat = dfs["/overlay"] or dfs["/"] or dfs["/rom"]
  local disks = {}

  for _, dev in ipairs(parsed.blockdevices or {}) do
    if dev.type == "disk" then
      local children = {}
      local system_root = is_system_mount(dev.mountpoint)
      for _, child in ipairs(dev.children or {}) do
        local mount = type(child.mountpoint) == "string" and child.mountpoint or nil
        local fstype = type(child.fstype) == "string" and child.fstype or nil
        if mount or fstype then
          local part = part_status(child, dfs)
          system_root = system_root or part.isSystemRoot
          children[#children + 1] = part
        end
      end
      if system_root or #children > 0 then
        disks[#disks + 1] = {
          name = dev.name or "",
          path = dev.path or (dev.name and ("/dev/" .. dev.name) or ""),
          venderModel = dev.name or "",
          size = human_size(dev.size),
          used = human_size(system_root and rootstat and rootstat.used or 0),
          usage = system_root and rootstat and rootstat.usage or 0,
          isSystemRoot = system_root,
          childrens = children
        }
      end
    end
  end

  http.write(jsonc.stringify({success = 0, result = {disks = disks}}))
end

local function is_disk_status_request()
  local uri = http.getenv("REQUEST_URI") or ""
  return uri:find("/cgi-bin/luci/istore/nas/disk/status", 1, true) ~= nil
end

local function read_number(path)
  local data = fs.readfile(path)
  return data and tonumber((data:gsub("%s+", ""))) or nil
end

local function format_uptime(seconds)
  local days = math.floor(seconds / 86400)
  local hours = math.floor((seconds % 86400) / 3600)
  local minutes = math.floor((seconds % 3600) / 60)
  if days > 0 then
    return string.format("%dd %02dh %02dm", days, hours, minutes)
  end
  return string.format("%02dh %02dm", hours, minutes)
end

local function read_cpu_times()
  local stat = fs.readfile("/proc/stat") or ""
  local line = stat:match("^(cpu%s+[^%n]+)")
  if not line then
    return nil, nil
  end
  local values = {}
  for value in line:gmatch("(%d+)") do
    values[#values + 1] = tonumber(value) or 0
  end
  local idle = (values[4] or 0) + (values[5] or 0)
  local total = 0
  for _, value in ipairs(values) do
    total = total + value
  end
  return total, idle
end

local function cpu_usage()
  local total_a, idle_a = read_cpu_times()
  if not total_a then
    return 0
  end
  nixio.nanosleep(0, 200000000)
  local total_b, idle_b = read_cpu_times()
  if not total_b then
    return 0
  end
  local total_delta = total_b - total_a
  local idle_delta = idle_b - idle_a
  if total_delta <= 0 then
    return 0
  end
  return math.floor(((total_delta - idle_delta) * 100 / total_delta) + 0.5)
end

local function system_status()
  local _, sdat = get_session()
  http.prepare_content("application/json")
  if not sdat then
    http.write(jsonc.stringify({success = -1001, error = "Forbidden"}))
    return
  end

  local meminfo = fs.readfile("/proc/meminfo") or ""
  local mem_total = tonumber(meminfo:match("MemTotal:%s+(%d+)")) or 0
  local mem_available = tonumber(meminfo:match("MemAvailable:%s+(%d+)")) or 0
  local uptime_raw = fs.readfile("/proc/uptime") or "0"
  local uptime = tonumber(uptime_raw:match("^(%S+)")) or 0
  local temp = read_number("/sys/class/thermal/thermal_zone0/temp") or 0
  if temp > 1000 then
    temp = math.floor((temp / 1000) + 0.5)
  end

  http.write(jsonc.stringify({
    success = 0,
    result = {
      cpuUsage = cpu_usage(),
      cpuTemperature = temp,
      memAvailable = mem_available * 1024,
      memAvailablePercentage = mem_total > 0 and math.floor(mem_available * 100 / mem_total) or 0,
      memTotal = mem_total * 1024,
      uptime = math.floor(uptime),
      uptimeHuman = format_uptime(uptime),
      localtime = os.date("%Y-%m-%d %H:%M:%S")
    }
  }))
end

local function is_system_status_request()
  local uri = http.getenv("REQUEST_URI") or ""
  return uri:find("/cgi-bin/luci/istore/system/status", 1, true) ~= nil
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

function istore_backend() 
  if is_system_status_request() then
    system_status()
    return
  end
  if is_disk_status_request() then
    disk_status()
    return
  end

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
  if chunked == 1 then
    ltn12.pump.all(chunksource(sock, body_buffer), http.write)
  else
    local body_source = ltn12.source.cat(ltn12.source.string(body_buffer), sock:blocksource())
    ltn12.pump.all(body_source, http.write)
  end

  sock:close()
end
