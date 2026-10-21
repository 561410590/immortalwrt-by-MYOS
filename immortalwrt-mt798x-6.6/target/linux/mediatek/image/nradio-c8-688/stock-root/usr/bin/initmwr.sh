#!/bin/sh
#by manper 20250529

if [ ! -f /usr/bin/modemflag.conf ]; then
    sleep 105
    #/usr/share/modem/v6.sh
    touch /usr/bin/modemflag.conf
    #reboot
    #exit 0
fi

IFNAME="usbwan"
IFNAMEV6="usbwanv6"
DEVICE="usb0"
filep="/tmp/usbwan.conf"
rm -f /www/luci-static/resources/view/status/include/21_ethinfo.js
exit 0
setup_blue4_interface() {
    uci delete network.$IFNAME 2>/dev/null
    uci delete network.$IFNAMEV6 2>/dev/null

    uci set network.$IFNAME="interface"
    uci set network.$IFNAME.device="$DEVICE"
    uci set network.$IFNAME.proto='dhcp'

    uci set network.$IFNAMEV6="interface"
    uci set network.$IFNAMEV6.device="$DEVICE"
    uci set network.$IFNAMEV6.proto='dhcpv6'
    uci set network.$IFNAMEV6.reqaddress='try'
    uci set network.$IFNAMEV6.reqprefix='auto'

    uci add_list firewall.@zone[1].network="$IFNAME"
    uci add_list firewall.@zone[1].network="$IFNAMEV6"

    uci commit network
    uci commit firewall

    touch "$filep"
    echo "接口已添加"
    #/etc/init.d/network reload
}

remove_blue4_interface() {
    uci delete network.$IFNAME 2>/dev/null
    uci delete network.$IFNAMEV6 2>/dev/null

    uci del_list firewall.@zone[1].network="$IFNAME" 2>/dev/null
    uci del_list firewall.@zone[1].network="$IFNAMEV6" 2>/dev/null

    uci commit network
    uci commit firewall

    [ -f "$filep" ] && rm -f "$filep"
    echo "接口已删除"
    #/etc/init.d/network reload
 
    sleep 50
    ifdown wwan
    ifup wwan
    ifdown wwanv6
    ifup wwanv6
}

#while true; do
    #if ip link show | grep -q "$DEVICE"; then
        #if [ ! -f "$filep" ]; then
            #setup_blue4_interface
        #fi
    #else
        #if [ -f "$filep" ]; then
            #remove_blue4_interface
        #fi
    #fi
    #sleep 10
#done

