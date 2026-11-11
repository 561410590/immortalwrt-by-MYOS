local sys = require("luci.sys")
local pcdata = require("luci.xml").pcdata

local m = Map("qiyougamebooster", translate("Qiyou Game Booster"),
	translate("Play console games online with less lag and more stability.")
	.. "<br />"
	.. translate("Supports PS, Switch, Xbox, PC, and mobile."))

local s = m:section(TypedSection, "qiyougamebooster")
s.anonymous = true
s.addremove = false

local state = sys.exec("/usr/bin/qiyougamebooster.sh status 2>/dev/null")
local version = sys.exec("/usr/bin/qiyougamebooster.sh version 2>/dev/null")
local status = s:option(DummyValue, "status", translate("Status"))
status.rawhtml = true
status.value = "<strong>" .. pcdata(version .. " " .. translate(state)) .. "</strong>"

local enabled = s:option(Flag, "enable", translate("Enable"))
enabled.default = enabled.disabled
enabled.rmempty = false

local instructions = s:option(DummyValue, "instructions", translate("Setup"))
instructions.rawhtml = true
instructions.value = '<img src="/qiyougamebooster.png" height="300" alt="'
	.. pcdata(translate("Qiyou Game Booster setup")) .. '"/>'

return m
