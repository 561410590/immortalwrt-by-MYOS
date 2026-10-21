-- Preserve bookmarks created before restoring the OAF 7 interface.
module("luci.controller.appfilter", package.seeall)

function index()
    entry({"admin", "services", "appfilter"},
        alias("admin", "services", "oaf", "dashboard"), nil).dependent = false
    entry({"admin", "services", "appfilter", "app_filter"},
        alias("admin", "services", "oaf", "app_filter", "rules"), nil).leaf = true
    entry({"admin", "services", "appfilter", "user_list"},
        alias("admin", "services", "oaf", "users", "list"), nil).leaf = true
    entry({"admin", "services", "appfilter", "feature"},
        alias("admin", "services", "oaf", "feature"), nil).leaf = true
    entry({"admin", "services", "appfilter", "advance"},
        alias("admin", "services", "oaf", "advanced"), nil).leaf = true
end
