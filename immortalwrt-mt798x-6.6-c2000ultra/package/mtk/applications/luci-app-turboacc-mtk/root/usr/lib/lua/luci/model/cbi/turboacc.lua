local kernel_version = luci.sys.exec("echo -n $(uname -r)")
local native_hnat = nixio.fs.access("/sys/kernel/debug/hnat/hook_toggle")

m = Map("turboacc")
m.title	= translate("Turbo ACC Acceleration Settings")
m.description = translate("Opensource Flow Offloading driver (Fast Path or Hardware NAT)")

m:append(Template("turboacc/turboacc_status"))

s = m:section(TypedSection, "turboacc", "")
s.addremove = false
s.anonymous = true

-- nftables: nft_flow_offload is built-in mod
if not native_hnat and nixio.fs.access("/lib/modules/" .. kernel_version .. "/nft_flow_offload.ko") then
sw_flow = s:option(Flag, "sw_flow", translate("Software flow offloading"))
sw_flow.default = 0
sw_flow.description = translate("Software based offloading for routing/NAT")
sw_flow:depends("sfe_flow", 0)
end

if native_hnat then
hw_flow = s:option(Flag, "hw_flow", translate("MediaTek HNAT"))
hw_flow.default = 1
hw_flow.rmempty = false
hw_flow.description = translate("Use native MediaTek hardware routing/NAT acceleration without nftables software flow offloading.")

for _, setting in ipairs({
	{ "hnat_ipv6", translate("IPv6 hardware acceleration") },
	{ "hnat_nf_stats", translate("Synchronize hardware counters") },
	{ "hnat_dscp", translate("DSCP priority mapping") }
}) do
	local o = s:option(ListValue, setting[1], setting[2])
	o:value("", translate("Keep current driver setting"))
	o:value("1", translate("Enabled"))
	o:value("0", translate("Disabled"))
	o:depends("hw_flow", 1)
end

local bind_rate = s:option(Value, "hnat_bind_rate", translate("HNAT bind rate threshold (pps)"))
bind_rate.datatype = "and(uinteger,range(1,30))"
bind_rate.placeholder = "30"
bind_rate.rmempty = true
bind_rate.description = translate("The smaller the threshold, the easier it is for the connection to be accelerated.")
bind_rate:depends("hw_flow", 1)

local qos = s:option(ListValue, "hnat_qos", translate("Hardware QoS mode"))
qos:value("", translate("Keep current driver setting"))
qos:value("0", translate("Disabled"))
qos:value("1", translate("Bidirectional HQoS"))
qos:value("1 uplink", translate("Uplink HQoS"))
qos:value("1 downlink", translate("Downlink HQoS"))
qos:value("2", translate("Per-port queues"))
qos:depends("hw_flow", 1)

local wireless = s:option(Button, "_wireless", translate("Wireless HNAT settings"))
wireless.inputtitle = translate("Open wireless settings")
wireless.inputstyle = "link"
wireless.write = function()
	luci.http.redirect(luci.dispatcher.build_url("admin", "network", "wireless"))
end
elseif luci.sys.call("cat /etc/openwrt_release | grep -Eq 'filogic|mt762' ") == 0 then
hw_flow = s:option(Flag, "hw_flow", translate("Hardware flow offloading"))
hw_flow.default = 0
hw_flow.description = translate("Requires hardware NAT support, implemented at least for mt762x")
hw_flow:depends("sw_flow", 1)
end

if luci.sys.call("cat /etc/openwrt_release | grep -Eq 'mediatek' ") == 0 then
if nixio.fs.access("/lib/modules/" .. kernel_version .. "/mt7915e.ko") then
hw_wed = s:option(Flag, "hw_wed", translate("MTK WED WO offloading"))
hw_wed.default = 0
hw_wed.description = translate("Requires hardware support, implemented at least for Filogic 8x0")
hw_wed:depends("hw_flow", 1)
end
end

if not native_hnat and (nixio.fs.access("/lib/modules/" .. kernel_version .. "/shortcut-fe-cm.ko")
or nixio.fs.access("/lib/modules/" .. kernel_version .. "/fast-classifier.ko"))
then
sfe_flow = s:option(Flag, "sfe_flow", translate("Shortcut-FE flow offloading"))
sfe_flow.default = 0
sfe_flow.description = translate("Shortcut-FE based offloading for routing/NAT")
sfe_flow:depends("sw_flow", 0)
end

-- nftables
if nixio.fs.access("/lib/modules/" .. kernel_version .. "/nft_fullcone.ko") then
fullcone_nat = s:option(Flag, "fullcone_nat", translate("FullCone NAT"))
fullcone_nat.default = 0
fullcone_nat.description = translate("Using FullCone NAT can improve gaming performance effectively")
fullcone6 = s:option(Flag, "fullcone6", translate("IPv6 Full Cone NAT"))
fullcone6.default = 0
fullcone6.description = translate("Enabling IPv6 Full Cone NAT adds an extra layer of NAT to IPv6. In IPv6, if you obtain an IPv6 prefix through IPv6 Prefix Delegation, each device can be assigned a public IPv6 address, eliminating the need for IPv6 Full Cone NAT.")
fullcone6:depends("fullcone_nat", 1)
end

if nixio.fs.access("/lib/modules/" .. kernel_version .. "/tcp_bbr.ko") then
bbr_cca = s:option(Flag, "bbr_cca", translate("BBR CCA"))
bbr_cca.default = 0
bbr_cca.description = translate("Using BBR CCA can improve TCP network performance effectively")
end

return m
