module("luci.controller.myos_n5000_peripherals", package.seeall)

local path = {"admin", "system", "myos_peripherals"}
local fs = require "nixio.fs"
local sys = require "luci.sys"
local json = require "luci.jsonc"
local cam = "/usr/libexec/mwrt-camsnap"
local audio = "/usr/libexec/mwrt-audio"

function index()
    local function item(name, action, title)
        local page = entry(name and {path[1], path[2], path[3], name} or path, action, title, 74)
        page.acl_depends = {"luci-mod-system-config"}
        if name then page.leaf = true end
    end
    item(nil, view("myos/peripherals"), _("USB 外接设备"))
    item("status", call("status"))
    item("command", post("command"))
    item("frame", call("frame"))
    item("levels", call("levels"))
end

local function run(binary, argument)
    return json.parse(sys.exec(binary.." "..argument.." 2>/dev/null") or "") or {ok=false}
end

local function reply(data)
    luci.http.header("Cache-Control", "no-store")
    luci.http.prepare_content("application/json")
    luci.http.write_json(data)
end

function status()
    reply({
        camera=run(cam,"list"),
        ffmpeg=fs.access("/usr/bin/ffmpeg","x") == true,
        audio=fs.access("/proc/asound/cards") and run(audio,"state") or {cards={}},
        qmodem=fs.access("/usr/share/luci/menu.d/luci-app-qmodem-next.json") == true
    })
end

function command()
    local action = luci.http.formvalue("action")
    if action == "camera_start" then return reply(run(cam,"start")) end
    if action == "camera_stop" then return reply(run(cam,"stop")) end

    local value = luci.http.formvalue("value") or ""
    local argument
    if action == "volume" and value:match("^%d%d?%d?$") and tonumber(value) <= 100 then
        argument = "vol play "..value
    elseif action == "mic" and (value == "0" or value == "1") then
        argument = "mic "..value
    elseif action == "play" and value:match("^[tus]%x%x%x%x%x%x%x%x$") then
        argument = "play "..value
    elseif action == "player" and (value == "pause" or value == "resume" or value == "stop") then
        argument = "ctl "..value
    end
    reply(argument and fs.access("/proc/asound/cards") and run(audio,argument) or {ok=false})
end

function frame()
    local beat, jpg = "/tmp/mwrt-cam.beat", "/tmp/mwrt-cam.jpg"
    luci.http.header("Cache-Control","no-store")
    if not fs.access(beat) then return luci.http.status(404,"Camera stopped") end
    fs.utimes(beat,os.time(),os.time())
    local info = fs.stat(jpg)
    if not info or info.size < 1 or info.size > 2097152 then
        return luci.http.status(503,"Frame unavailable")
    end
    luci.http.prepare_content("image/jpeg")
    luci.http.write(fs.readfile(jpg) or "")
end

function levels()
    if not fs.access("/proc/asound/cards") then return reply({ok=false}) end
    reply(run(audio,"wave"))
end
