local M = {}

function M.visible()
    return require("luci.model.uci").cursor():get("mwrt", "access", "unlocked") == "1"
end

return M
