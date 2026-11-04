local M = {}
function M.unlock()
    local http, fs = require "luci.http", require "nixio.fs"
    local code = http.formvalue("set")
    local ok = code == "88888888" or code == "666666"
    if ok then
        ok = fs.writefile("/etc/config/Outofnetwork", "ok\n") ~= nil
        if ok then
            for name in fs.glob("/tmp/luci-indexcache*") do fs.unlink(name) end
            require("luci.sys").call("rm -rf /tmp/luci-modulecache")
        end
    end
    http.prepare_content("application/json")
    http.write_json({resdd=ok and "yes" or "no"})
end
return M

