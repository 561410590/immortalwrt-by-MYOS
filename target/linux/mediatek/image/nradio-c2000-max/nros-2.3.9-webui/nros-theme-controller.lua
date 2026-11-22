module("luci.controller.nradio_adv.myos_theme", package.seeall)

function index()
    local page = entry({"nradioadv", "system", "myos_theme"},
        template("nradio_adv/myos_theme"), "主题切换", 45, true)
    page.show = true
    page.icon = "cog"
end
