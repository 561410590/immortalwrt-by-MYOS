local uci = luci.model.uci.cursor()
m = Map("qmodem_ttl", translate("TTL 配置"))
s = m:section(NamedSection, "main", "main", translate("全局配置"))

enable = s:option(Flag, "enable", translate("启用"))
enable.default = "0"

ttl = s:option(Value, "ttl", translate("TTL"))
ttl.default = 64
ttl.datatype = "range(1,255)"


function m.on_after_commit()
    luci.sys.call("/etc/init.d/qmodem_ttl reload >/dev/null 2>&1")
end

return m
