local M = {}

function M.theme()
    return require("luci.model.uci").cursor():get("luci", "main", "mediaurlbase") == "/luci-static/myos-new"
end

function M.visible()
    return M.theme() and require("luci.model.uci").cursor():get("mwrt", "access", "unlocked") == "1"
end

function M.unlock()
    local http = require "luci.http"
    local function reply(value)
        http.prepare_content("application/json")
        http.write_json({resdd=value})
    end
    if not require("luci.dispatcher").test_post_security() then return end
    if not M.theme() then
        http.status(403, "Forbidden")
        return reply("denied")
    end
    if http.formvalue("set") ~= "666666" then return reply("no") end
    local config = require("luci.model.uci").cursor()
    config:set("mwrt", "access", "access")
    config:set("mwrt", "access", "unlocked", "1")
    if not config:commit("mwrt") then return reply("failed") end
    os.execute("rm -f /tmp/luci-indexcache* 2>/dev/null; rm -rf /tmp/luci-modulecache 2>/dev/null")
    return reply("yes")
end

return M
