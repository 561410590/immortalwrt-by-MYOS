#!/bin/sh
IFNAME="BLUE4POE"
IFNAMEV6="BLUE4V6POE"
filep="/usr/bin/blue4wan-AK68.conf"

#python3 /usr/bin/websocket_server-AK68.py &
if [ -f "$filep" ]; then
    echo "检测到已经初始化过，跳过wan口配置。"
    exit 0
fi

#echo "部署webui服务中..."
#tar -xf /usr/share/modem-AK68/5700.tar.xz -C /www/ && rm /usr/share/modem-AK68/5700.tar.xz
#/etc/init.d/uhttpd restart
#echo "安装smspdudecoder-2.1.1-py3-none-any..."
#pip3 install --no-index --find-links=/usr/share/modem-AK68 /usr/share/modem-AK68/bitarray-3.0.0.tar.gz
#pip3 install --no-index --find-links=/usr/share/modem-AK68 /usr/share/modem-AK68/bitstring-4.3.0-py3-none-any.whl
#pip3 install --no-index --find-links=/usr/share/modem-AK68 /usr/share/modem-AK68/smspdudecoder-2.1.1-py3-none-any.whl
#echo "安装smspdudecoder-2.1.1-py3-none-any完成."
#python3 /usr/bin/websocket_server-AK68.py &
#echo "uhttpd服务重启完成！"
#echo "WEBUI安装完成."

# 检查是否存在名为BLUE4的网络接口
if ! ip link show | grep -q "BLUE4"; then
    echo "Network interface BLUE4 not found, exiting."
    echo "未找到相应网络接口，该设备可能不是鲲鹏设备，可能需要手动配置WAN口..."
    exit 1
fi

setup_blue4_interface() {
    # 配置 BLUE4WAN IPv4 接口
    uci del_list network.@device[0].ports='BLUE4'
    uci set network.$IFNAME="interface"
    uci set network.$IFNAME.ifname="BLUE4"
    uci set network.$IFNAME.proto='dhcp'
    uci add_list firewall.@zone[1].network=$IFNAME

    # 配置 BLUE4WANV6 IPv6 接口
    #uci delete network.$IFNAMEV6
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

setup_blue4_interface
