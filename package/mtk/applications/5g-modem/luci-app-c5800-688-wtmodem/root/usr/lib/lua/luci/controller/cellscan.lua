module("luci.controller.cellscan", package.seeall)

local function detected_model()
    local file = io.open("/tmp/modconf.conf", "r")
    if file then
        local content = file:read("*all") or ""
        file:close()
        if content ~= "" then return content end
    end

    file = io.open("/tmp/infocd/cache/cpestatus", "r")
    if not file then return "" end
    local content = file:read("*all") or ""
    file:close()
    -- Only the internal modem owns these scan commands.
    local section = content:match('"name"%s*:%s*"cpe"(.-)"name"%s*:%s*"cpe1"')
        or content:match('"name"%s*:%s*"cpe"(.*)') or ""
    return section:match('"model"%s*:%s*"([^"]+)"') or ""
end

function index()
    -- entry({"admin", "modem"}, firstchild(), _("蜂窝"), 25).dependent=false
	local content = detected_model()
	local template_name = "cellscan/cellscan_null"
	if content ~= "" then
		if content and string.find(content, "RM520") then
			template_name = "cellscan/cellscan"
		elseif content and string.find(content, "RM500U") then
			template_name = "cellscan/cellscan_null"
        elseif content and string.find(content, "MT5700") then
			template_name = "cellscan/cellscan_mt5700m"    
		end
	end
    --entry({"admin", "modem", "cellscan"}, template("cellscan/cellscan"), _("邻区扫描"), 80).dependent = true
    entry({"admin", "modem", "cellscan"}, template(template_name), _("邻区扫描"), 80).dependent = true
    entry({"admin", "modem", "cellscan", "switch2"}, call("action_switch2"), nil)
    entry({"admin", "modem", "cellscan", "switch5g"}, call("action_switch5g"), nil)
    entry({"admin", "modem", "cellscan", "switch4g"}, call("action_switch4g"), nil)
    entry({"admin", "modem", "cellscan", "snapshot"}, call("action_snapshot"), nil)
end

function action_snapshot()
    local http = require "luci.http"
    local iface = http.formvalue("iface") or "cpe"
    http.prepare_content("application/json")
    http.write_json(require("c5800_radio").snapshot(iface))
end

function action_switch2()
    if detected_model():find("MT5700", 1, true) then return action_snapshot() end
    local sys = require "luci.sys"
    local http = require "luci.http"
    local confirm = http.formvalue("confirm")
    
    if confirm and confirm == "yes" then
        --sys.call("./usr/share/modem/keyPairCellScan.sh")
        luci.http.redirect(luci.dispatcher.build_url("admin", "modem", "cellscan"))
        os.execute("/usr/share/modem/keyPairCellScan.sh")
    else
        luci.http.redirect(luci.dispatcher.build_url("admin", "modem", "cellscan"))
    end
end
function action_switch5g()
    local sys = require "luci.sys"
    local http = require "luci.http"
    local confirm = http.formvalue("confirm")
    
    if confirm and confirm == "yes" then
        if detected_model():find("MT5700", 1, true) then return action_snapshot() end
        luci.http.redirect(luci.dispatcher.build_url("admin", "modem", "cellscan"))
        local content = detected_model()
        if content ~= "" then
            if content and string.find(content, "RM520") then
                os.execute("/usr/share/modem/keyPairCellScan.sh 5")
            elseif content and string.find(content, "MT5700") then
                return action_snapshot()
            end
        end    
    else
        luci.http.redirect(luci.dispatcher.build_url("admin", "modem", "cellscan"))
    end
end
function action_switch4g()
    local sys = require "luci.sys"
    local http = require "luci.http"
    local confirm = http.formvalue("confirm")
    
    if confirm and confirm == "yes" then
        --sys.call("./usr/share/modem/keyPairCellScan.sh")
        if detected_model():find("MT5700", 1, true) then return action_snapshot() end
        luci.http.redirect(luci.dispatcher.build_url("admin", "modem", "cellscan"))
        local content = detected_model()
        if content ~= "" then
            if content and string.find(content, "RM520") then
                os.execute("/usr/share/modem/keyPairCellScan.sh 4")
            elseif content and string.find(content, "MT5700") then
                return action_snapshot()
            end
        end 
    else
        luci.http.redirect(luci.dispatcher.build_url("admin", "modem", "cellscan"))
    end
end


function parse_results()
    local results = {}
    local controller = {}
    -- Read and parse cellinfo file
    local cellinfo = io.open("/tmp/kpcellinfo", "r")
    if cellinfo then
        for line in cellinfo:lines() do
            local mode, operator, band, earfcn, pci, rsrp, rsrq = line:match('+QSCAN: "(.-)",(.-),(.-),(.-),(.-),(.-),(.+)')
            if mode and operator and earfcn and pci and rsrp and rsrq then
                table.insert(controller, {
                    mode = mode,
                    operator = operator,
                    band = band,
                    earfcn = earfcn,
                    pci = pci,
                    rsrp = rsrp,
                    rsrq = rsrq
                })
            end
        end
        cellinfo:close()
    else
        table.insert(controller, {
            mode = "wait for ctrl...",
            operator = "",
            band = "",
            earfcn = "",
            pci = "",
            rsrp = "",
            rsrq = ""
        })
    end
    return controller
end

function cellscan_run_time()
    local file = io.open("/tmp/cellscan_run_time", "r")
    cellscan_time="1"
    if file then
        cellscan_time=file:read("*all")
        io.close(file)
    end
    return cellscan_time
end
