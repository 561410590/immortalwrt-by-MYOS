#!/bin/sh
DEV="BLUE4"
IFACE4="BLUE4POE"
IFACE6="BLUE4POEV6"
METRIC=10

if ! ip link show | grep -q "BLUE4"; then
    echo "Network interface BLUE4 not found, exiting."
    echo "未找到相应网络接口，该设备可能不是鲲鹏设备，可能需要手动配置WAN口..."
    exit 1
fi


check_ipv6_pd() {
    ipv6=$(ip -6 addr show dev "$DEV" scope global | grep -oE "([0-9a-fA-F]{0,4}:){2,7}[0-9a-fA-F]{0,4}" | head -n 1)
    if [ -n "$ipv6" ]; then
        echo "1" 
    else
        echo "0" 
    fi
}

set_network() {
    uci -q delete network.$IFACE4
    uci -q delete network.$IFACE6

    # IPv4 WAN
    uci set network.$IFACE4='interface'
    uci set network.$IFACE4.proto='dhcp'
    uci set network.$IFACE4.ifname="$DEV"
    uci set network.$IFACE4.device="$DEV"
    uci set network.$IFACE4.metric="$METRIC"

    # IPv6 WAN6
    uci set network.$IFACE6='interface'
    uci set network.$IFACE6.proto='dhcpv6'
    uci set network.$IFACE6.ifname="@$IFACE4"
    uci set network.$IFACE6.device="@$IFACE4"
    uci set network.$IFACE6.metric="$METRIC"

    uci set network.lan.ipv6='1'
    uci set network.lan.ip6assign='64'

    uci set dhcp.$IFACE6='dhcp'
    uci set dhcp.$IFACE6.interface="$IFACE6"
    uci set dhcp.$IFACE6.ra='relay'
    uci set dhcp.$IFACE6.ndp='relay'
    uci set dhcp.$IFACE6.master='1'
    uci set dhcp.$IFACE6.ignore='1'

    uci set dhcp.lan.ra='relay'
    uci set dhcp.lan.ndp='relay'
    uci set dhcp.lan.dhcpv6='relay'

    WAN_ZONE_IDX=$(uci show firewall | grep -n "=zone" | grep "'wan'" | cut -d: -f1 | head -n 1)
    if [ -z "$WAN_ZONE_IDX" ]; then
        #uci add firewall zone
        #uci set firewall.@zone[-1].name="wan"
        #uci set firewall.@zone[-1].input="REJECT"
        #uci set firewall.@zone[-1].output="ACCEPT"
        #uci set firewall.@zone[-1].forward="REJECT"
        #uci set firewall.@zone[-1].masq="1"
        #uci set firewall.@zone[-1].mtu_fix="1"
        #uci add_list firewall.@zone[-1].network="$IFACE4"
        #uci add_list firewall.@zone[-1].network="$IFACE6"
        uci add_list firewall.@zone[1].network="$IFACE4"
        uci add_list firewall.@zone[1].network="$IFACE6"
    else
        uci add_list firewall.@zone[1].network="$IFACE4"
        uci add_list firewall.@zone[1].network="$IFACE6"
    fi

    uci commit network
    uci commit dhcp
    uci commit firewall

    #/etc/init.d/network restart
    #/etc/init.d/firewall restart
    #/etc/init.d/odhcpd restart
   
    echo "Done"
}


set_network

