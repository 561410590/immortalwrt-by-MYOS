module("luci.controller.acc", package.seeall)

-- 引擎是官方 acc-gw.router(与 plugin_install.sh 装出来的一样), 绑定/加速/设备管理全在雷神 APP 里做。
-- 这里只做三件事: 看运行状态、启停服务、定时暂停(雷神账号按时长计费, 到点自动暂停)。

local fs = require "nixio.fs"
local HELPER = "/usr/libexec/leigod-pause"

function index()
  -- APP 里「卸载插件」会删掉 /etc/init.d/acc, 菜单跟着隐藏
  if not fs.access("/etc/init.d/acc") then
    return
  end
  entry({ "admin", "services", "acc" }, alias("admin", "services", "acc", "service"), _("Leigod Acc"), 50).dependent = true
  entry({ "admin", "services", "acc", "service" }, cbi("leigod/service"), _("Leigod Service"), 10).leaf = true
  entry({ "admin", "services", "acc", "app" }, template("leigod/app"), _("Leigod App"), 20).leaf = true
  entry({ "admin", "services", "acc", "status" }, call("action_status")).leaf = true
  -- 启停会改开机自启和防火墙规则, 只收带 token 的 POST
  entry({ "admin", "services", "acc", "control" }, post("action_control")).leaf = true
end

local function read_ini_field(path, field)
  local data = fs.readfile(path) or ""
  local v = data:match("\n" .. field .. "=([^\n]*)") or data:match("^" .. field .. "=([^\n]*)")
  if v then
    v = v:gsub("^%s*\"?", ""):gsub("\"?%s*$", "")
  end
  return v
end

-- 进程名 acc-gw.router.<arch> 超过 15 字符, comm 被截断, ps/pidof 不可靠, 跟官方脚本一样扫 cmdline
local function scan_procs()
  local r = { daemon = false, web = false, upgrade = false, acc = {} }
  for pid in fs.dir("/proc") or function() end do
    if pid:match("^%d+$") then
      local cmd = fs.readfile("/proc/" .. pid .. "/cmdline")
      if cmd and (cmd:find("acc-gw.router", 1, true) or cmd:find("acc_upgrade_monitor", 1, true)) then
        cmd = cmd:gsub("%z", " ")
        local role = cmd:match("%-r (%S+)")
        if role == "daemon" or role == "web" or role == "upgrade" then
          r[role] = true
        elseif role == "acc" then
          local t = cmd:match("%-t (%S+)")
          if t then r.acc[#r.acc + 1] = t end
        end
      end
    end
  end
  table.sort(r.acc)
  return r
end

-- /proc/net/{tcp,udp}: 第 2 列本地地址 IP:PORT(十六进制), 第 4 列状态; tcp 0A=LISTEN, udp 07=已绑定
local function port_open(proto, port)
  local want = (proto == "tcp") and "0A" or "07"
  local hex = string.format("%04X", port)
  for line in (fs.readfile("/proc/net/" .. proto) or ""):gmatch("[^\n]+") do
    local lport, st = line:match("^%s*%d+:%s+%x+:(%x+)%s+%S+%s+(%x+)")
    if lport and lport:upper() == hex and st:upper() == want then
      return true
    end
  end
  return false
end

function action_status()
  local sys = require "luci.sys"
  local ini = "/etc/config/accelerator.ini"
  local data = fs.readfile(ini) or ""
  local p = scan_procs()

  -- 只给布尔值和计数, token/user_id/设备 MAC 不出接口
  local token = data:match("\ntoken=\"?([^\"\n]*)")
  local devices = 0
  local dev = data:match("\n%[device%]\n(.*)$") or ""
  for line in dev:gmatch("[^\n]+") do
    if line:match("^%[") then break end
    if line:match("=") then devices = devices + 1 end
  end

  local resp = {
    enabled = sys.init.enabled("acc"),
    daemon = p.daemon,
    web = p.web,
    upgrade = p.upgrade,
    accelerating = p.acc,
    port_api = port_open("tcp", 5588),
    port_discover = port_open("udp", 6066),
    bound = (token ~= nil and #token > 0),
    devices = devices,
    version = read_ini_field("/etc/config/acc_version.ini", "version") or "",
    mode = read_ini_field("/etc/config/acc_firewall.ini", "mode") or "",
    backend = read_ini_field("/etc/config/acc_firewall.ini", "backend") or ""
  }
  luci.http.prepare_content("application/json")
  luci.http.write_json(resp)
end

function action_control()
  local op = luci.http.formvalue("op")
  local cmd = ({
    -- autostart 记在 uci 里: 保留配置升级时 99-leigod-acc 据此决定开不开机自启
    start = "uci -q set leigod.base=base; uci -q set leigod.base.autostart=1; uci -q commit leigod; /etc/init.d/acc enable; /etc/init.d/acc restart",
    restart = "/etc/init.d/acc restart",
    stop = "uci -q set leigod.base=base; uci -q set leigod.base.autostart=0; uci -q commit leigod; /etc/init.d/acc stop; /etc/init.d/acc disable"
  })[op]
  luci.http.prepare_content("application/json")
  if not cmd then
    luci.http.status(400, "Bad Request")
    luci.http.write_json({ result = "bad op" })
    return
  end
  os.execute(cmd .. " >/dev/null 2>&1")
  luci.http.write_json({ result = "OK" })
end

-- 按 leigod.base 的定时暂停设置重写 crontab 里那一行。
-- cron 行只放时间, 账号密码由 leigod-pause 运行时从 uci 读, 不进 crontab;
-- crontab 用 Lua 直接读写, 不经 shell 拼接。旧版路径 /usr/sbin/leigod/leigod-helper.sh 的行一并清掉。
function update_pause_cron()
  local uci = require "luci.model.uci".cursor()

  local schedule_enabled = uci:get("leigod", "base", "schedule_enabled") or "0"
  local pause_time = uci:get("leigod", "base", "pause_time") or "01:00"

  local crontab = "/etc/crontabs/root"
  local kept = {}
  for line in (fs.readfile(crontab) or ""):gmatch("[^\n]+") do
    if not line:find(HELPER, 1, true) and not line:find("/usr/sbin/leigod/leigod-helper.sh", 1, true) then
      kept[#kept + 1] = line
    end
  end

  if schedule_enabled == "1" then
    local hour, minute = pause_time:match("^(%d%d?):(%d%d)$")
    hour, minute = tonumber(hour), tonumber(minute)
    if hour and minute and hour < 24 and minute < 60 then
      kept[#kept + 1] = string.format("%d %d * * * %s", minute, hour, HELPER)
    end
  end

  fs.mkdirr("/etc/crontabs")
  fs.writefile(crontab, #kept > 0 and (table.concat(kept, "\n") .. "\n") or "")
  os.execute("/etc/init.d/cron restart >/dev/null 2>&1")
end
