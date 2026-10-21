#!/bin/sh

nbcpe_optional_led() {
    local path="/sys/class/leds/hc:blue:$1/brightness"
    [ -w "$path" ] || return 0
    echo "$2" > "$path"
}
lock_file="/var/run/network_status_update-AK68.lock"
exec 200>$lock_file
flock -n 200 || exit 1

nbcpe_mt5700_odu() {
    local driver alias
    [ "$(uci -q get network.cpe.mode)" = "odu" ] || return 1
    driver=$(jsonfilter -i /tmp/infocd/cache/cpe_dev -e '@.parameter.driver' 2>/dev/null)
    alias=$(jsonfilter -i /tmp/infocd/cache/cpe_dev -e '@.parameter.alias' 2>/dev/null)
    [ "$driver:$alias" = "odu:mt5700" ]
}

# NBCPE shares one AT collector with the homepage and APP. LED updates must
# never open another AT connection or interpret an unsolicited response as loss.
nbcpe_led_write() {
    local path="/sys/class/leds/hc:blue:$1/brightness" current
    [ -w "$path" ] || return 0
    read -r current < "$path"
    [ "$current" = "$2" ] || printf '%s\n' "$2" > "$path"
}

nbcpe_cached_leds() {
    local status age registered mode sample last_sample= bad=0
    local selected=0 last_good=0 now uptime led wifi dev
    for led in status wifi cmode4 cmode5; do
        [ -w "/sys/class/leds/hc:blue:$led/trigger" ] &&
            echo none > "/sys/class/leds/hc:blue:$led/trigger"
    done
    while true; do
        echo MT5700 > /tmp/modconf-AK68.conf
        read -r uptime _ < /proc/uptime
        now=${uptime%%.*}
        status=$(/usr/libexec/myos-nbcpe-status 2>/dev/null)
        age=$(printf '%s' "$status" | jsonfilter -e '@.radio_age' 2>/dev/null)
        registered=$(printf '%s' "$status" | jsonfilter -e '@.registration.registered' 2>/dev/null)
        mode=$(printf '%s' "$status" | jsonfilter -e '@.summary.network_mode' 2>/dev/null)
        sample=$(jsonfilter -i /tmp/myos-nbcpe-radio.json -e '@.sampled_at' 2>/dev/null)
        case "$age" in ''|*[!0-9]*) age=999 ;; esac
        if [ "$age" -le 90 ] && [ "$registered" = true ]; then
            case "$mode" in
                *NR*|*5G*) selected=5; last_good=$now; bad=0 ;;
                *LTE*|*4G*) selected=4; last_good=$now; bad=0 ;;
            esac
        elif [ "$age" -le 90 ] && [ "$registered" = false ]; then
            # Count new radio samples, rather than rereading one failed sample.
            if [ -n "$sample" ] && [ "$sample" != "$last_sample" ]; then
                bad=$((bad + 1))
            fi
            [ "$bad" -lt 2 ] || selected=0
        fi
        last_sample=$sample
        [ "$((now - last_good))" -le 120 ] || selected=0
        if [ -f /tmp/ledflag.conf ] || [ -f /usr/bin/ledflagc.conf ]; then
            for led in status wifi cmode4 cmode5; do nbcpe_led_write "$led" 0; done
        else
            nbcpe_led_write status 1
            wifi=0
            for dev in ra0 rax0 wlan0 wlan1; do
                [ -r "/sys/class/net/$dev/operstate" ] || continue
                read -r state < "/sys/class/net/$dev/operstate"
                [ "$state" = up ] && wifi=1
            done
            nbcpe_led_write wifi "$wifi"
            if [ "$selected" = 5 ]; then
                nbcpe_led_write cmode4 0
                nbcpe_led_write cmode5 1
            elif [ "$selected" = 4 ]; then
                nbcpe_led_write cmode5 0
                nbcpe_led_write cmode4 1
            else
                nbcpe_led_write cmode5 0
                nbcpe_led_write cmode4 0
            fi
        fi
        sleep 5
    done
}

if [ -x /usr/libexec/myos-nbcpe-status ] && nbcpe_mt5700_odu; then
    nbcpe_cached_leds
    exit
fi

while true
do
    sleep 8
    if nbcpe_mt5700_odu; then
        echo "MT5700" > /tmp/modconf-AK68.conf
    elif ping -c 1 192.168.225.1 > /dev/null 2>&1; then
    	output=$(atsd_tools_cli -i cpe -c "ATI")
        if echo "$output" | grep -q "RM520"; then
            echo "RM520N" > /tmp/modconf-AK68.conf
        else
            if ping -c 1 192.168.8.1 > /dev/null 2>&1; then
                output=$(atsd_tools_cli -i cpe -c "ATI")
                if echo "$output" | grep -q "MT5700"; then
                    echo "MT5700" > /tmp/modconf-AK68.conf
                fi
            else
                echo "AK68套件断开或未接入！" > /tmp/modconf-AK68.conf
                echo "设备检测中..." > /tmp/devck.conf
                echo 0 > /sys/class/leds/hc:blue:cmode5/brightness
                echo 0 > /sys/class/leds/hc:blue:cmode4/brightness
                nbcpe_optional_led sig1 0
                nbcpe_optional_led sig2 0
                nbcpe_optional_led sig3 0
                nbcpe_optional_led int 1
                sleep 1
                continue
            fi
        fi
    elif ping -c 1 192.168.8.1 > /dev/null 2>&1; then
          output=$(atsd_tools_cli -i cpe -c "ATI")
          if echo "$output" | grep -q "MT5700"; then
              echo "MT5700" > /tmp/modconf-AK68.conf
          fi
    else
        echo "AK68套件断开或未接入！" > /tmp/modconf-AK68.conf
        echo "设备检测中..." > /tmp/devck.conf
        echo 0 > /sys/class/leds/hc:blue:cmode5/brightness
        echo 0 > /sys/class/leds/hc:blue:cmode4/brightness
        nbcpe_optional_led sig1 0
        nbcpe_optional_led sig2 0
        nbcpe_optional_led sig3 0
        nbcpe_optional_led int 1
        sleep 1
        continue
    fi
    #不和内置模块LED灯冲突
    if pgrep -f "netmodeled.sh" > /dev/null; then
    	continue
    else
        echo "内置LED灯控制器未运行，接管控制。"
    fi
    #判断不同模组执行不同的判断
    modconf=$(cat /tmp/modconf-AK68.conf)
    if [ ! -f "/tmp/ledflag.conf" ]; then
        if echo "$modconf" | grep -q "RM520N"; then
            network_mode=$(atsd_tools_cli -i cpe -c at+qnwinfo | grep '+QNWINFO' | awk -F\" '{print $2}' | tr -d '\r')
            OX=$( atsd_tools_cli -i cpe -c "AT+CSQ" |grep "+CSQ:")
            OX=$(echo $OX | tr 'a-z' 'A-Z')
            CSQ=$(echo "$OX" | grep -o "+CSQ: [0-9]\{1,2\}" | grep -o "[0-9]\{1,2\}")
            if [ $CSQ = "99" ]; then
                CSQ="0"
                echo 0 > /sys/class/leds/hc:blue:cmode5/brightness
                echo 0 > /sys/class/leds/hc:blue:cmode4/brightness
                nbcpe_optional_led sig1 0
                nbcpe_optional_led sig2 0
                nbcpe_optional_led sig3 0
                nbcpe_optional_led int 1
            fi
            if [ -n "$CSQ" ]; then
                RSRQ=$(($CSQ * 100/31))
                if [ "$RSRQ" -ge 99 ]; then
                    nbcpe_optional_led sig1 1
                    nbcpe_optional_led sig2 1
                    nbcpe_optional_led sig3 1
                elif [ "$RSRQ" -ge 95 ] && [ "$RSRQ" -lt 99 ]; then
                    nbcpe_optional_led sig1 1
                    nbcpe_optional_led sig2 1
                    nbcpe_optional_led sig3 1
                elif [ "$RSRQ" -ge 90 ] && [ "$RSRQ" -lt 95 ]; then
                    nbcpe_optional_led sig1 1
                    nbcpe_optional_led sig2 1
                    nbcpe_optional_led sig3 1
                elif [ "$RSRQ" -ge 85 ] && [ "$RSRQ" -lt 90 ]; then
                    nbcpe_optional_led sig1 1
                    nbcpe_optional_led sig2 1
                    nbcpe_optional_led sig3 1
                elif [ "$RSRQ" -ge -80 ] && [ "$RSRQ" -lt 85 ]; then
                    nbcpe_optional_led sig1 1
                    nbcpe_optional_led sig2 1
                    nbcpe_optional_led sig3 1
                elif [ "$RSRQ" -ge 65 ] && [ "$RSRQ" -lt 80 ]; then
                    nbcpe_optional_led sig1 1
                    nbcpe_optional_led sig2 1
                    nbcpe_optional_led sig3 0
                elif [ "$RSRQ" -ge 1 ] && [ "$RSRQ" -lt 65 ]; then
                    nbcpe_optional_led sig1 1
                    nbcpe_optional_led sig2 0
                    nbcpe_optional_led sig3 0
                else
                    nbcpe_optional_led sig1 1
                    nbcpe_optional_led sig2 0
                    nbcpe_optional_led sig3 0
               fi
            else
                echo 0 > /sys/class/leds/hc:blue:cmode5/brightness
                echo 0 > /sys/class/leds/hc:blue:cmode4/brightness
                nbcpe_optional_led sig1 0
                nbcpe_optional_led sig2 0
                nbcpe_optional_led sig3 0
                nbcpe_optional_led int 1
            fi
            # 判断网络模式并更新LED状态
            if echo "$network_mode" | grep -Eq "5G|NR"; then
                # 包含TDD，点亮hc:blue:cmode5，熄灭hc:blue:cmode4
                echo 1 > /sys/class/leds/hc:blue:cmode5/brightness
                echo 0 > /sys/class/leds/hc:blue:cmode4/brightness
                nbcpe_optional_led int 0
            elif echo "$network_mode" | grep -q "LTE"; then
                # 包含LTE，点亮hc:blue:cmode4，熄灭hc:blue:cmode5
                echo 1 > /sys/class/leds/hc:blue:cmode4/brightness
                echo 0 > /sys/class/leds/hc:blue:cmode5/brightness
                nbcpe_optional_led int 0
            else
                # 其他网络，熄灭hc:blue:cmode5和hc:blue:cmode4
                echo 0 > /sys/class/leds/hc:blue:cmode5/brightness
                echo 0 > /sys/class/leds/hc:blue:cmode4/brightness
                nbcpe_optional_led sig1 0
                nbcpe_optional_led sig2 0
                nbcpe_optional_led sig3 0
                nbcpe_optional_led int 1
            fi
            
        elif echo "$modconf" | grep -q "MT5700"; then
            network_mode=$(atsd_tools_cli -i cpe -c 'AT^HCSQ?')
            OUTPUT=$(atsd_tools_cli -i cpe -c 'AT^MONSC' | sed -n '2p')
            # 检查输出是否包含NR（即网络类型为NR）
            echo $OUTPUT | grep 'NR' &> /dev/null
            if [ $? -ne 0 ]; then
                echo $OUTPUT | grep 'LTE' &> /dev/null
                if [ $? -ne 0 ]; then
                    echo 0 > /sys/class/leds/hc:blue:cmode5/brightness
                    echo 0 > /sys/class/leds/hc:blue:cmode4/brightness
                    nbcpe_optional_led sig1 0
                    nbcpe_optional_led sig2 0
                    nbcpe_optional_led sig3 0
                    nbcpe_optional_led int 1
                    continue
                else
                    #LTE 4G解析数据
                    RSRQ=$(echo $OUTPUT | cut -d ',' -f9)
                    RSRQ=$(echo $OUTPUT | cut -d ',' -f9 | sed 's/[^-.0-9]//g')
                    # 根据 RSRQ 的值设置 CSQ_PER
                    if [ "$RSRQ" -ge -3 ]; then
                        nbcpe_optional_led sig1 1
                        nbcpe_optional_led sig2 1
                        nbcpe_optional_led sig3 1
                    elif [ "$RSRQ" -ge -5 ] && [ "$RSRQ" -lt -3 ]; then
                        nbcpe_optional_led sig1 1
                        nbcpe_optional_led sig2 1
                        nbcpe_optional_led sig3 1
                    elif [ "$RSRQ" -ge -10 ] && [ "$RSRQ" -lt -5 ]; then
                        nbcpe_optional_led sig1 1
                        nbcpe_optional_led sig2 1
                        nbcpe_optional_led sig3 1
                    elif [ "$RSRQ" -ge -11 ] && [ "$RSRQ" -lt -10 ]; then
                        nbcpe_optional_led sig1 1
                        nbcpe_optional_led sig2 1
                        nbcpe_optional_led sig3 1
                    elif [ "$RSRQ" -ge -12 ] && [ "$RSRQ" -lt -11 ]; then
                        nbcpe_optional_led sig1 1
                        nbcpe_optional_led sig2 1
                        nbcpe_optional_led sig3 1
                    elif [ "$RSRQ" -ge -15 ] && [ "$RSRQ" -lt -12 ]; then
                        nbcpe_optional_led sig1 1
                        nbcpe_optional_led sig2 1
                        nbcpe_optional_led sig3 0
                    elif [ "$RSRQ" -ge -17 ] && [ "$RSRQ" -lt -15 ]; then
                        nbcpe_optional_led sig1 1
                        nbcpe_optional_led sig2 0
                        nbcpe_optional_led sig3 0
                    else
                        nbcpe_optional_led sig1 1
                        nbcpe_optional_led sig2 0
                        nbcpe_optional_led sig3 0
                    fi
                fi
            else
                RSRQ=$(echo $OUTPUT | cut -d ',' -f10)
                RSRQ=$(echo $OUTPUT | cut -d ',' -f10 | sed 's/[^-.0-9]//g')
                if [ "$RSRQ" -ge -3 ]; then
                    nbcpe_optional_led sig1 1
                    nbcpe_optional_led sig2 1
                    nbcpe_optional_led sig3 1
                elif [ "$RSRQ" -ge -5 ] && [ "$RSRQ" -lt -3 ]; then
                    nbcpe_optional_led sig1 1
                    nbcpe_optional_led sig2 1
                    nbcpe_optional_led sig3 1
                elif [ "$RSRQ" -ge -10 ] && [ "$RSRQ" -lt -5 ]; then
                    nbcpe_optional_led sig1 1
                    nbcpe_optional_led sig2 1
                    nbcpe_optional_led sig3 1
                elif [ "$RSRQ" -ge -11 ] && [ "$RSRQ" -lt -10 ]; then
                    nbcpe_optional_led sig1 1
                    nbcpe_optional_led sig2 1
                    nbcpe_optional_led sig3 1
                elif [ "$RSRQ" -ge -12 ] && [ "$RSRQ" -lt -11 ]; then
                    nbcpe_optional_led sig1 1
                    nbcpe_optional_led sig2 1
                    nbcpe_optional_led sig3 1
                elif [ "$RSRQ" -ge -15 ] && [ "$RSRQ" -lt -12 ]; then
                    nbcpe_optional_led sig1 1
                    nbcpe_optional_led sig2 1
                    nbcpe_optional_led sig3 0
                elif [ "$RSRQ" -ge -17 ] && [ "$RSRQ" -lt -15 ]; then
                    nbcpe_optional_led sig1 1
                    nbcpe_optional_led sig2 0
                    nbcpe_optional_led sig3 0
                else
                    nbcpe_optional_led sig1 1
                    nbcpe_optional_led sig2 0
                    nbcpe_optional_led sig3 0
                fi
            fi
        # 判断网络模式并更新LED状态
        if echo "$network_mode" | grep -Eq "5G|NR"; then
            # 包含TDD，点亮hc:blue:cmode5，熄灭hc:blue:cmode4
            echo 1 > /sys/class/leds/hc:blue:cmode5/brightness
            echo 0 > /sys/class/leds/hc:blue:cmode4/brightness
            nbcpe_optional_led int 0
        elif echo "$network_mode" | grep -q "LTE"; then
            # 包含LTE，点亮hc:blue:cmode4，熄灭hc:blue:cmode5
            echo 1 > /sys/class/leds/hc:blue:cmode4/brightness
            echo 0 > /sys/class/leds/hc:blue:cmode5/brightness
            nbcpe_optional_led int 0
        else
            # 其他网络，熄灭hc:blue:cmode5和hc:blue:cmode4
            echo 0 > /sys/class/leds/hc:blue:cmode5/brightness
            echo 0 > /sys/class/leds/hc:blue:cmode4/brightness
            nbcpe_optional_led sig1 0
            nbcpe_optional_led sig2 0
            nbcpe_optional_led sig3 0
            nbcpe_optional_led int 1
        fi
    fi
  fi
done

# 释放锁
flock -u 200
