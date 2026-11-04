local M = {}

local function trim(value)
    return tostring(value or ""):match("^%s*(.-)%s*$")
end

function M.normalize_mac(value)
    local mac = trim(value):upper():gsub("%-MYOS$", ""):gsub(":", "")
    if not mac:match("^%x%x%x%x%x%x%x%x%x%x%x%x$") then return "" end
    if mac == "000000000000" or mac == "FFFFFFFFFFFF" or mac == "006688000000" then return "" end
    if tonumber(mac:sub(1, 2), 16) % 2 ~= 0 then return "" end
    return mac:gsub("(%x%x)", "%1:"):sub(1, 17)
end

local function read_file(path, count)
    local file = io.open(path, "rb")
    if not file then return nil end
    local raw = file:read(count)
    file:close()
    return raw
end

function M.decode_bdinfo(raw)
    if type(raw) ~= "string" or #raw ~= 65536 then return nil end
    local version, present = raw:byte(4), raw:byte(5)
    local offset = version == 1 and 384 or version == 2 and 15 or nil
    if not offset or not present or present == 0 or present == 255 then return nil end
    -- libbdinfo.so.3.3: version at byte 3; v1 text at 0x180, v2 at 0x0f.
    local payload = raw:sub(offset + 1):match("^[^%z\255]*")
    if not payload or not payload:find("=", 1, true) then return nil end
    local result = {}
    for line in (payload .. "\n"):gmatch("([^\r\n]*)[\r\n]+") do
        if trim(line) ~= "" then
            local key, value = line:match("^%s*([A-Za-z0-9_]+)%s*=%s*(.-)%s*$")
            if not key or #key > 64 or #value > 512 or result[key] ~= nil then return nil end
            if value:find("[%z\1-\8\11\12\14-\31\127]") then return nil end
            result[key] = value
        end
    end
    return result
end

function M.is_c8_record(record)
    return type(record) == "table" and trim(record.board):upper() == "WT9104" and
        trim(record.oem_pname):upper() == "C8-688" and trim(record.oem_vendor):lower() == "nradio"
end

function M.patch_eeprom(raw, mac)
    mac = M.normalize_mac(mac)
    if type(raw) ~= "string" or #raw ~= 4096 or raw:byte(1) ~= 0x81 or
        raw:byte(2) ~= 0x79 or mac == "" then return nil end
    local bytes = mac:gsub("(%x%x):?", function(value) return string.char(tonumber(value, 16)) end)
    return raw:sub(1, 4) .. bytes .. raw:sub(11)
end

local function block_paths()
    local result, seen = {}, {}
    local function add(path)
        path = trim(path)
        if path:match("^/dev/[A-Za-z0-9_./-]+$") and not seen[path] then
            result[#result + 1], seen[path] = path, true
        end
    end
    add("/dev/disk/by-partlabel/bdinfo")
    local file = io.open("/proc/mtd", "r")
    if file then
        for line in file:lines() do
            local number, name = line:match('^mtd(%d+):%s+%x+%s+%x+%s+"([^"]+)"')
            if number and (name == "bdinfo" or name == "oeminfo") then add("/dev/mtd" .. number) end
        end
        file:close()
    end
    local pipe = io.popen("blkid -t PARTLABEL=bdinfo -o device 2>/dev/null")
    if pipe then
        for line in pipe:lines() do add(line) end
        pipe:close()
    end
    return result
end

function M.hardware()
    for _, path in ipairs(block_paths()) do
        local record = M.decode_bdinfo(read_file(path, 65536))
        if M.is_c8_record(record) and
            (M.normalize_mac(record.fac_mac) ~= "" or M.normalize_mac(record.mac) ~= "") then
            return record
        end
    end
    return {}
end

local allowed = { fac_mac = true, mac = true, device_code = true, remote_password = true,
    oem_pname = true, oem_ptype = true, oem_vendor = true }

function M.get(key)
    if not allowed[key] then return "" end
    return trim(M.hardware()[key])
end

local function cursor()
    return require("luci.model.uci").cursor()
end

function M.mac()
    local uci = cursor()
    local manual = M.normalize_mac(uci:get("myos_nros", "main", "manual_mac"))
    if manual ~= "" then return manual end
    local record = M.hardware()
    local mac = M.normalize_mac(record.fac_mac)
    return mac ~= "" and mac or M.normalize_mac(record.mac)
end

function M.device_id()
    return M.mac():gsub(":", "")
end

local function valid_code(value)
    return type(value) == "string" and value ~= "" and value:lower() ~= "x11111" and
        #value <= 128 and value:match("^[%w_.%-]+$") ~= nil
end

function M.device_code()
    local factory = M.get("device_code")
    if valid_code(factory) then return factory end
    local uci = cursor()
    local adopted = uci:get("myos_nros", "main", "adopted_device_code")
    if valid_code(adopted) and M.device_id() ~= "" and
        uci:get("myos_nros", "main", "adopted_device_id") == M.device_id() then return adopted end
    return ""
end

function M.adopt_device_code(value)
    value = trim(value)
    local current, id = M.device_code(), M.device_id()
    if current ~= "" then return current == value end
    if not valid_code(value) or id == "" then return false end
    local uci = cursor()
    if not uci:get("myos_nros", "main") then uci:section("myos_nros", "cloud", "main", {}) end
    return uci:set("myos_nros", "main", "adopted_device_code", value) and
        uci:set("myos_nros", "main", "adopted_device_id", id) and uci:commit("myos_nros") and
        M.device_code() == value
end

return M
