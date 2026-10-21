BEGIN {
	inserted_helpers = 0
	patched_setup_macs = 0
	patched_lan_mac = 0
	patched_cpe_mac = 0
	patched_wan_mac = 0
	patched_lan = 0
	patched_wan = 0
	patched_ovlan_query = 0
	patched_nbcpe_query = 0
	patched_detect_switch = 0
	patched_empty_msw = 0
}

/^[[:space:]]*_ovlan=\$\(uci get network\.nrswitch\.ovlan\)/ && !patched_ovlan_query {
	gsub(/uci get/, "uci -q get")
	patched_ovlan_query = 1
}

/^[[:space:]]*_nbcpe=\$\(uci get network\.globals\.nbcpe\)/ && !patched_nbcpe_query {
	gsub(/uci get/, "uci -q get")
	patched_nbcpe_query = 1
}

/^detect_switch\(\) \{/ && !patched_detect_switch {
	print
	print "\tcommand -v swconfig >/dev/null 2>&1 || return 0"
	patched_detect_switch = 1
	next
}

/^[[:space:]]*if \[ "\$_port" != "\$GMAC1_PHY_PORT" \]; then/ && !patched_empty_msw {
	print "\tif [ -n \"$_port\" ] && [ \"$_port\" != \"$GMAC1_PHY_PORT\" ]; then"
	patched_empty_msw = 1
	next
}

/^[[:space:]]*if ! uci -q get "network\.\$_ifname"; then/ && !patched_setup_macs {
	print "\t\tif c2000max_board && [ \"$_ifname\" = \"eth1\" ]; then"
	print "\t\t\tcontinue"
	print "\t\tfi"
	patched_setup_macs = 1
}

/^[[:space:]]*uci -q set "network\.\$\{_ifname\}\.macaddr=\$_lan_mac"/ && !patched_lan_mac {
	print "\t\tif c2000max_board && [ \"$_ifname\" = \"eth1\" ]; then"
	print "\t\t\tcontinue"
	print "\t\tfi"
	patched_lan_mac = 1
}

/^[[:space:]]*uci -q set "network\.\$\{_ifname\}\.macaddr=\$_cpe_mac"/ && !patched_cpe_mac {
	print "\t\tif c2000max_board && [ \"$_ifname\" = \"eth1\" ]; then"
	print "\t\t\tcontinue"
	print "\t\tfi"
	patched_cpe_mac = 1
}

/^[[:space:]]*uci -q set "network\.\$\{_ifname\}\.macaddr=\$_wan_mac"/ && !patched_wan_mac {
	print "\t\tif c2000max_board && [ \"$_ifname\" = \"eth1\" ]; then"
	print "\t\t\tcontinue"
	print "\t\tfi"
	patched_wan_mac = 1
}

!inserted_helpers && /^set_switch_lan_dsa\(\) \{/ {
	print "c2000max_board() {"
	print "\tcase \"$(cat /tmp/sysinfo/board_name 2>/dev/null)\" in"
	print "\t\tnradio,c2000-max|HCMT7987-EMMC|hcmt7987-emmc) return 0 ;;"
	print "\tesac"
	print "\treturn 1"
	print "}"
	print ""
	print "c2000max_check_eth1_wan_iface() {"
	print "\tlocal _section=\"$1\""
	print "\tlocal _device _ifname _disabled"
	print "\t[ \"$_section\" = \"lan\" ] && return"
	print "\tconfig_get _disabled \"$_section\" disabled"
	print "\t[ \"$_disabled\" = \"1\" ] && return"
	print "\tconfig_get _device \"$_section\" device"
	print "\tconfig_get _ifname \"$_section\" ifname"
	print "\t[ \"$_device\" = \"eth1\" ] || [ \"$_ifname\" = \"eth1\" ] || return"
	print "\tgC2000MAXEth1Wan=1"
	print "}"
	print ""
	print "c2000max_eth1_wan_enabled() {"
	print "\tc2000max_board || return 1"
	print "\tgC2000MAXEth1Wan=0"
	print "\tconfig_load network"
	print "\tconfig_foreach c2000max_check_eth1_wan_iface interface"
	print "\t[ \"$gC2000MAXEth1Wan\" = \"1\" ]"
	print "}"
	print ""
	print "c2000max_restore_lan_dhcp() {"
	print "\tuci -q get dhcp.lan >/dev/null || uci -q set dhcp.lan=dhcp"
	print "\tuci -q get dhcp.lan.interface >/dev/null || uci -q set dhcp.lan.interface=lan"
	print "\tuci -q get dhcp.lan.start >/dev/null || uci -q set dhcp.lan.start=100"
	print "\tuci -q get dhcp.lan.limit >/dev/null || uci -q set dhcp.lan.limit=150"
	print "\tuci -q get dhcp.lan.leasetime >/dev/null || uci -q set dhcp.lan.leasetime=12h"
	print "\tuci -q delete dhcp.lan.ignore"
	print "\tuci -q commit dhcp"
	print "}"
	print ""
	inserted_helpers = 1
}

/^[[:space:]]*if \[ "\$_ports" = "\$_cur_ports" \]; then/ && !patched_lan {
	print "\tif c2000max_board; then"
	print "\t\tif c2000max_eth1_wan_enabled; then"
	print "\t\t\t_ports=\"hnat\""
	print "\t\telse"
	print "\t\t\t_ports=\"eth1 hnat\""
	print "\t\t\tuci -q delete network.eth1"
	print "\t\t\tc2000max_restore_lan_dhcp"
	print "\t\tfi"
	print "\tfi"
	patched_lan = 1
}

/^[[:space:]]*_wan_port=\$\(get_ports "\$SWITCH_WAN_PORT_TYPES"\)/ && !patched_wan {
	print
	print ""
	print "\tif c2000max_board; then"
	print "\t\treturn"
	print "\tfi"
	patched_wan = 1
	next
}

{ print }

END {
	if (!inserted_helpers || !patched_setup_macs || !patched_lan_mac || !patched_cpe_mac || !patched_wan_mac || !patched_lan || !patched_wan || !patched_ovlan_query || !patched_nbcpe_query || !patched_detect_switch || !patched_empty_msw) {
		exit 1
	}
}
