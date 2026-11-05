local M = {}

function M.theme()
    return require("luci.model.uci").cursor():get("luci", "main", "mediaurlbase") == "/luci-static/myos-new"
end

function M.visible()
    return M.theme() and require("luci.model.uci").cursor():get("mwrt", "access", "unlocked") == "1"
end

return M
