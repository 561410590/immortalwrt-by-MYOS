-- 运行状态 + 启停按钮在 leigod/status 模板里; 下面只有定时暂停的配置(uci leigod.base)。
-- 引擎自己的 /etc/config/accelerator.ini 不是 uci 格式, 不能拿 Map 去读写。

m = Map("leigod", translate("Leigod Accelerator"),
  translate("Bind the router, choose devices and start acceleration in the Leigod APP. The phone must be connected to this router's LAN or WiFi."))

m:section(SimpleSection).template = "leigod/status"

s = m:section(NamedSection, "base", "base", translate("Scheduled Pause"))
s.addremove = false

schedule_enabled = s:option(Flag, "schedule_enabled", translate("Scheduled Pause"))
schedule_enabled.rmempty = false
schedule_enabled.default = "0"
schedule_enabled.description = translate("Leigod is billed by time. When enabled, the account is paused automatically at the chosen time every day.")

pause_time = s:option(ListValue, "pause_time", translate("Pause Time"))
pause_time:depends("schedule_enabled", "1")
for i = 0, 23 do
  pause_time:value(string.format("%02d:00", i))
end
pause_time.default = "01:00"

username = s:option(Value, "username", translate("Phone Number"))
username:depends("schedule_enabled", "1")
username.datatype = "and(uinteger,minlength(5),maxlength(20))"

password = s:option(Value, "password", translate("Leigod Password"))
password:depends("schedule_enabled", "1")
password.password = true

m.on_after_commit = function(self)
  require("luci.controller.acc").update_pause_cron()
end

return m
