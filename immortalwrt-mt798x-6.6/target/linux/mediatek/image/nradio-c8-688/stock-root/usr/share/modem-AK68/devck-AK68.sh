#!/bin/sh
# 检查是否已经有锁文件存在
lock_file="/var/run/devck_status-AK68.lock"
exec 200>$lock_file
flock -n 200 || exit 1
while true
do
    # 检查 /tmp/devck.conf 文件内容
    sleep 15
    if [ -f /tmp/devck.conf ] && grep -q "已有设备" /tmp/devck.conf; then
        sleep 15
        continue
    fi

    if ping -c 1 192.168.225.1 > /dev/null 2>&1; then
        cp -f /usr/share/modem-AK68/C-RM520N /etc/config/atsd_tools
        /etc/init.d/atsd_tools restart
        sleep 1
        output=$(atsd_tools_cli -i cpe -c "ATI")
        if echo "$output" | grep -q "RM520"; then
            echo "已有设备" > /tmp/devck.conf
            echo "RM520N" > /tmp/modconf-AK68.conf
            echo "AK68已连接" > /tmp/lucimsg.file
            sleep 2
            /usr/share/modem-AK68/rm520n-AK68.sh &
        else
            echo "检测到IP 192.168.225.1,但该IP不是X62模组！已跳过。"
            if ping -c 1 192.168.8.1 > /dev/null 2>&1; then
                cp -f /usr/share/modem-AK68/C-MT5700 /etc/config/atsd_tools
                /etc/init.d/atsd_tools restart
                sleep 1
                output=$(atsd_tools_cli -i cpe -c "ATI")
                if echo "$output" | grep -q "MT5700"; then
                    echo "已有设备" > /tmp/devck.conf
                    echo "AK68已连接" > /tmp/lucimsg.file
                    echo "MT5700" > /tmp/modconf-AK68.conf
                    sleep 2
                    /usr/share/modem-AK68/MT5700-AK68.sh &
                else
                    echo "检测到IP 192.168.8.1,但该IP不是巴龙模组！已跳过。"
                    sleep 1
                    continue
                fi
            else
                echo "AK68套件断开或未接入！" > /tmp/modconf-AK68.conf
                echo 0 > /sys/class/leds/hc:blue:cmode5/brightness
                echo 0 > /sys/class/leds/hc:blue:cmode4/brightness
                sleep 1
                continue
            fi
        fi
    elif ping -c 1 192.168.8.1 > /dev/null 2>&1; then
            cp -f /usr/share/modem-AK68/C-MT5700 /etc/config/atsd_tools
            /etc/init.d/atsd_tools restart
            sleep 1
            output=$(atsd_tools_cli -i cpe -c "ATI")
            if echo "$output" | grep -q "MT5700"; then
                echo "已有设备" > /tmp/devck.conf
                echo "MT5700" > /tmp/modconf-AK68.conf
                echo "AK68已连接" > /tmp/lucimsg.file
                sleep 2
                /usr/share/modem-AK68/MT5700-AK68.sh &
            else
                echo "检测到IP 192.168.8.1,但该IP不是巴龙模组！已跳过。"
                sleep 1
                continue
            fi
    else
        echo "AK68套件断开或未接入！" > /tmp/modconf-AK68.conf
        echo 0 > /sys/class/leds/hc:blue:cmode5/brightness
        echo 0 > /sys/class/leds/hc:blue:cmode4/brightness
        sleep 1
        continue
    fi
done
# 释放锁
flock -u 200
