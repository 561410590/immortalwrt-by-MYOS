#!/bin/sh
# NBCPE has its own WebUI and netifd ODU services. Legacy bootstrap must not
# unpack the bundled 2025 WebUI over the installed version or open a second AT server.
if [ -x /etc/init.d/myos-nbcpe-webui ] && [ -x /etc/init.d/myos-nbcpe-cache ]; then
    exit 0
fi
IFNAME="BLUE4WAN"
IFNAMEV6="BLUE4WANV6"
filep="/usr/bin/blue4wan-AK68.conf"

python3 /usr/bin/websocket_server-AK68.py &
if [ -f "$filep" ]; then
    echo "back"
    exit 0
fi

echo "部署webui服务中..."
tar -xf /usr/share/modem-AK68/5700.tar.xz -C /www/ && rm /usr/share/modem-AK68/5700.tar.xz
/etc/init.d/uhttpd restart
echo "检查smspdudecoder-2.1.1..."
python3 -c 'import bitarray, bitarray.util, bitstring, smspdudecoder' || exit 1
echo "smspdudecoder-2.1.1已预装."
python3 /usr/bin/websocket_server-AK68.py &
echo "uhttpd服务重启完成！"
echo "WEBUI安装完成."

# 检查是否存在名为BLUE4的网络接口
if ! ip link show | grep -q "BLUE4"; then
    echo "Network interface BLUE4 not found, exiting."
    echo "未找到相应网络接口，该设备可能不是鲲鹏设备，可能需要手动配置WAN口..."
    exit 1
fi

setup_blue4_interface() {
    # 配置 BLUE4WAN IPv4 接口
    uci delete network.$IFNAME
    uci set network.$IFNAME="interface"
    uci set network.$IFNAME.ifname="BLUE4"
    uci set network.$IFNAME.proto='dhcp'
    uci add_list firewall.@zone[1].network=$IFNAME

    # 配置 BLUE4WANV6 IPv6 接口
    uci delete network.$IFNAMEV6
    uci set network.$IFNAMEV6="interface"
    uci set network.$IFNAMEV6.ifname="BLUE4"
    uci set network.$IFNAMEV6.proto='dhcpv6'
    uci add_list firewall.@zone[1].network=$IFNAMEV6

    # 提交更改
    uci commit network
    uci commit firewall

    # 创建标志文件，防止重复执行
    touch "$filep"
    echo "网口配置完成."
}

#setup_blue4_interface
