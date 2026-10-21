local fs = require "nixio.fs"
local json = require "luci.jsonc"
local M = {}

function M.is_nbcpe()
    local board = (fs.readfile("/tmp/sysinfo/board_name") or ""):gsub("%s+$", "")
    return board == "HCMT7981-emmc-NB68" or board == "nradio_wt9111_No2-manper_NB68"
end

function M.status()
    local empty = { section = "AK68", cache_only = true, sources = {} }
    if not M.is_nbcpe() or not fs.access("/usr/libexec/myos-nbcpe-status") then return empty end
    local pipe = io.popen("/usr/libexec/myos-nbcpe-status 2>/dev/null")
    if not pipe then return empty end
    local out = pipe:read("*a") or ""
    pipe:close()
    if #out > 1048576 then return empty end
    local data = json.parse(out)
    return type(data) == "table" and type(data.sources) == "table" and data or empty
end

function M.entries(data)
    local entries = {}
    for _, source in ipairs(data.sources or {}) do
        local age = tonumber(source.age) or 0
        for _, entry in ipairs(source.entries or {}) do
            local key = tostring(entry.key or "")
            local upper = key:upper()
            local class = "Cell Information"
            if upper == "NAME" or upper == "REVISION" or upper == "MANUFACTURER" or
               upper == "TEMPERATURE" or upper == "NETWORK_MODE" or upper == "CONNECT_STATUS" or upper == "ISP" then
                class = "Base Information"
            elseif upper == "ICCID" or upper == "IMSI" or upper:match("^SIM") then
                class = "SIM Information"
            end
            if entry.value ~= nil then
                entries[#entries + 1] = { key = key, full_name = key, class = class,
                    value = tostring(entry.value), stale = age > 120 and age or nil,
                    extra_info = entry.extra_info }
            end
        end
    end
    if data.sim_label then
        entries[#entries + 1] = { key = "SIM_SLOT", full_name = "SIM Slot", class = "SIM Information", value = data.sim_label }
    end
    return entries
end

return M
