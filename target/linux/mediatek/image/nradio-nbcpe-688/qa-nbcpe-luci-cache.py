#!/usr/bin/env python3
"""Compare stock LuCI cache responses while forbidding command execution."""

import argparse
import json
import subprocess
from pathlib import Path


STOCK_HANDLER = r'''
local function stock()
    local conf_file_path = "/tmp/modconf-AK68.conf"
    local conf_file = io.open(conf_file_path, "r")
    local modem_type = nil

    if conf_file then
        modem_type = conf_file:read("*line")
        conf_file:close()
    else
        error("Unable to open configuration file: " .. conf_file_path)
    end

    if modem_type:find("RM520") then
        io.popen("/usr/share/modem-AK68/zinfo-AK68.sh")
    elseif modem_type:find("MT5700") then
        io.popen("/usr/share/modem-AK68/zinfo5700-AK68.sh")
    else
        error("Unsupported modem type")
    end

    local file, file2
    local stat = "/tmp/cpe_cell-AK68.file"
    local stat2 = "/tmp/stsss.file"

    file = io.open(stat, "r")
    file2 = io.open(stat2, "r")

    if not file or not file2 then
        error("Error opening status files.")
    end

    local rv = {}
    rv["stsss"] = file2:read("*line")
    rv["modem"] = file:read("*line")
    rv["conntype"] = file:read("*line")
    rv["firmware"] = file:read("*line")
    rv["temper"] = file:read("*line")
    rv["date"] = file:read("*line")
	--------------------------------
	rv["simsel"] = file:read("*line")
	rv["cops"] = file:read("*line")
	rv["imei"] = file:read("*line")
	rv["imsi"] = file:read("*line")
	rv["iccid"] = file:read("*line")
	rv["phone"] = file:read("*line")
	--------------------------------
	rv["mode"] = file:read("*line")
	rv["per"] = file:read("*line")
	rv["rssi"] = file:read("*line")
	rv["rsrq"] = file:read("*line")
	rv["rscp"] = file:read("*line")
	rv["sinr"] = file:read("*line")
	-------------------------------
	rv["mcc"] = file:read("*line")
	rv["lac"] = file:read("*line")
	rv["cid"] = file:read("*line")
	rv["band"] = file:read("*line")
	rv["rfcn"] = file:read("*line")
	rv["pci"] = file:read("*line")
	rv["apn"] = file:read("*line")
	rv["down"] = file:read("*line")
	rv["up"] = file:read("*line")
	rv["qci"] = file:read("*line")
	rv["zbjh"] = file:read("*line")
	rv["r2cc"] = file:read("*line")
	rv["r3cc"] = file:read("*line")
	rv["cell1"] = file:read("*line")
	rv["cell2"] = file:read("*line")
	rv["cell3"] = file:read("*line")
	rv["cell4"] = file:read("*line")
	rv["cell5"] = file:read("*line")
	--------------------------------
	file:close()
    	file2:close()
	luci.http.prepare_content("application/json")
	luci.http.write_json(rv)
end
'''


FIXTURE = r'''
local fixed_path = FIXED
local current, result, calls, handles = {}, nil, 0, {}
package.loaded['nixio.fs'] = {}
_G.luci = {http = {
    prepare_content = function(value) assert(value == 'application/json') end,
    write_json = function(value) result = value end
}}
local function forbidden() error('cache endpoint attempted command execution') end
os.execute = forbidden
io.open = function(path, mode)
    assert(mode == 'r', 'cache endpoint attempted a write')
    local rows = current[path]
    if not rows then return nil end
    local handle = {position = 0, closed = false}
    handles[#handles + 1] = handle
    function handle:read(format)
        assert(format == '*line', 'unexpected cache read format')
        self.position = self.position + 1
        return rows[self.position]
    end
    function handle:close() self.closed = true; return true end
    return handle
end
assert(loadfile(fixed_path))()
local fixed = package.loaded['luci.controller.modem-AK68'].action_get_csq
local function invoke(handler, rows, modem, stock_mode)
    current = {['/tmp/cpe_cell-AK68.file'] = rows,
               ['/tmp/stsss.file'] = {'stock collector ready'}}
    if modem then current['/tmp/modconf-AK68.conf'] = {modem} end
    result, calls, handles = nil, 0, {}
    io.popen = stock_mode and function()
        calls = calls + 1; return {}
    end or forbidden
    handler()
    assert(type(result) == 'table', 'JSON response missing')
    for _, handle in ipairs(handles) do assert(handle.closed, 'cache handle leaked') end
    return result, calls
end
local function equal(expected, actual)
    for key, value in pairs(expected) do
        assert(actual[key] == value, 'changed stock field: ' .. key)
    end
    for key in pairs(actual) do assert(expected[key] ~= nil, 'added field: ' .. key) end
end
local rows = {}
for index = 1, 35 do rows[index] = 'row-' .. index end
for _, modem in ipairs({'MT5700', 'RM520N-CN'}) do
    local expected, count = invoke(stock, rows, modem, true)
    assert(count == 1, 'baseline no longer exercises the discarded pipe')
    local actual = invoke(fixed, rows, modem, false)
    equal(expected, actual)
    local without_type = invoke(fixed, rows, nil, false)
    equal(expected, without_type)
end
local no_sim = {'未检测到信息', 'MT5700M-CN', 'TD Tech Ltd.'}
for index = 4, 35 do no_sim[index] = '' end
equal(invoke(stock, no_sim, 'MT5700', true), invoke(fixed, no_sim, nil, false))
print('PASS: stock fields preserved for both modem types and no SIM; no commands; no type-file dependency')
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--assets', type=Path, default=Path(__file__).resolve().parent)
    parser.add_argument('--lua', required=True, type=Path)
    args = parser.parse_args()
    relative = Path('usr/lib/lua/luci/controller/modem-AK68.lua')
    fixed = args.assets / 'overlay' / relative
    prefix = 'local FIXED=' + json.dumps(str(fixed), ensure_ascii=False) + '\n'
    result = subprocess.run([str(args.lua), '-'], input=prefix + STOCK_HANDLER + FIXTURE,
                            text=True, encoding='utf-8', capture_output=True,
                            check=False, timeout=10)
    if result.returncode:
        raise RuntimeError(result.stderr or result.stdout or 'Lua fixture failed without diagnostics')
    model = (args.assets / 'overlay/usr/lib/lua/luci/model/cbi/modem5700-AK68.lua').read_text(encoding='utf-8')
    active = '\n'.join(line for line in model.splitlines() if not line.lstrip().startswith('--'))
    assert 'cat /tmp/simcardstat-AK68' not in active, 'obsolete SIM cache read remains'
    assert 'AT^SIMSQ?' in active, 'original SIM query was changed'
    print(result.stdout.strip())
    print('PASS: redundant SIM cache read removed; original SIM query retained')


if __name__ == '__main__':
    main()
