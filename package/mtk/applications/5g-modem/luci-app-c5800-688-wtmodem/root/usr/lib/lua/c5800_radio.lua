-- MT5700 AT manual 13.9/13.10: query serving and neighbour cells in service.
-- CELLSCAN needs data service stopped; do not use it in this online view.
local M = {}
local fs = require "nixio.fs"
local nixio = require "nixio"
local json = require "luci.jsonc"
local sys = require "luci.sys"

local function fields(line)
    local result = {}
    for value in (line .. ","):gmatch("(.-),") do
        result[#result + 1] = value:match("^%s*(.-)%s*$")
    end
    return result
end

local function query(iface, command)
    return sys.exec("cpetools.sh -i " .. iface .. " -t 0 -c " ..
        require("luci.util").shellquote(command) .. " 2>/dev/null")
end

local function signal(value, invalid)
    local number = tonumber(value)
    if number and number ~= invalid then return number end
end

local function collect(iface)
    local serving = query(iface, "AT^MONSC")
    local frequency = query(iface, "AT^HFREQINFO?")
    local neighbours = query(iface, "AT^MONNC")
    local data = { iface = iface, updated = os.time(), neighbours = {},
        raw = serving .. frequency .. neighbours }
    local band = fields(frequency:match("%^HFREQINFO:%s*([^\r\n]+)") or "")
    local cell = fields(serving:match("%^MONSC:%s*([^\r\n]+)") or "")
    if cell[1] == "NR" or cell[1] == "LTE" then
        local nr = cell[1] == "NR"
        local offset = nr and 1 or 0
        data.serving = { rat = cell[1], plmn = (cell[2] or "") .. (cell[3] or ""),
            arfcn = tonumber(cell[4]), scs = nr and tonumber(cell[5]) or nil, cell_id = cell[5 + offset],
            pci = tonumber(cell[6 + offset], 16), tac = cell[7 + offset],
            rsrp = signal(cell[8 + offset], -1256), rsrq = signal(cell[9 + offset], -348),
            sinr = nr and signal(cell[11], -188) or nil }
        -- HFREQINFO gives the carrier band; MONSC gives the NR SSB frequency.
        if band[2] == (nr and "7" or "6") and tonumber(band[3]) then
            data.serving.band = (nr and "n" or "B") .. band[3]
            data.serving.downlink_arfcn = tonumber(band[4])
            data.serving.bandwidth_mhz = (tonumber(band[6]) or 0) / 1000
        end
    else
        data.error = "未取得驻留小区，模组可能正在重选或没有服务"
    end
    local seen = {}
    for line in neighbours:gmatch("%^MONNC:%s*([^\r\n]+)") do
        local item = fields(line)
        if item[1] == "NR" or item[1] == "LTE" then
            local key = item[1] .. ":" .. tostring(item[2]) .. ":" .. tostring(item[3])
            if not seen[key] then
                seen[key] = true
                local hex = item[1] == "NR" or (item[3] or ""):find("[a-fA-F]")
                data.neighbours[#data.neighbours + 1] = { rat = item[1],
                    arfcn = tonumber(item[2]), pci = tonumber(item[3], hex and 16 or 10),
                    rsrp = signal(item[4], -1256), rsrq = signal(item[5], -348),
                    sinr = item[1] == "NR" and signal(item[6], -188) or nil }
            end
        end
    end
    data.neighbours_available = neighbours:find("%^MONNC:") ~= nil
    local uci = require("luci.model.uci").cursor()
    local sim = uci:get("cpesel", iface == "cpe" and "sim" or "sim1", "cur") or "1"
    local section = iface .. "sim" .. sim
    data.mode = uci:get("cpecfg", section, "mode") or "auto"
    data.frequency = uci:get("cpecfg", section, "freq") or ""
    return data
end

function M.snapshot(iface)
    if iface ~= "cpe" and iface ~= "cpe1" then return { error = "模组参数无效" } end
    if iface == "cpe1" and require("c5800_secondary").is_family() then
        local state = require("c5800_secondary").status()
        local s = state.signal
        local data = {iface=iface,native=true,updated=os.time(),neighbours={},
            raw=json.stringify(s),message="原厂副设备状态；原厂接口未提供邻区列表"}
        if s.cell or s.cellid or s.pci or s.earfcn then
            data.serving = {rat=s.mode or "",band=s.band,cell_id=s.cell or s.cellid,
                arfcn=s.earfcn or s.dl_fcn,pci=s.pci,rsrp=s.rsrp or s.RSRP,
                sinr=s.sinr or s.SINR,tac=s.tac}
        end
        return data
    end
    if iface == "cpe1" and fs.readfile("/sys/class/net/port5/carrier") ~= "1\n" then
        return { iface = iface, error = "PoE 扩展口未连接" }
    end
    local path = "/tmp/c5800-radio-" .. iface
    local cached = json.parse(fs.readfile(path .. ".json") or "")
    if cached and cached.updated and os.time() - cached.updated < 20 then return cached end
    -- Process-owned locks are released even when an HTTP worker is interrupted.
    local mutex = nixio.open(path .. ".lck", "w", "600")
    if not mutex then return { iface = iface, error = "无法打开模组读取锁" } end
    if not mutex:lock("tlock") then
        mutex:close()
        return { iface = iface, error = "正在读取此模组，请稍后刷新" }
    end
    local ok, data = pcall(collect, iface)
    mutex:close()
    if not ok then return { iface = iface, error = "小区读取失败" } end
    if data.serving then fs.writefile(path .. ".json", json.stringify(data)) end
    return data
end

return M
