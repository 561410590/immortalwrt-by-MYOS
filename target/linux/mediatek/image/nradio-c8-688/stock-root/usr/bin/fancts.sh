#!/bin/sh
#by manper 20250117
lockfile="/var/run/cooling_control.pid"

if [ -e "$lockfile" ]; then
    pid=$(cat "$lockfile")
    if kill -0 $pid > /dev/null 2>&1; then
        echo "Script is already running with PID $pid."
        exit 1
    else
        echo "Lock file found but no process running with PID $pid. Removing lock file."
        rm "$lockfile"
    fi
fi

echo $$ > "$lockfile"

trap "rm -f '$lockfile'; exit" INT TERM EXIT

echo disabled > /sys/class/thermal/thermal_zone0/mode
fanvall=$(cat /etc/fanvall)
    
if [ "$fanvall" = 3 ]; then
    echo 255 > /sys/devices/platform/pwm-fan/hwmon/hwmon3/pwm1
    echo 255 > /sys/devices/platform/pwm-fan/hwmon/hwmon1/pwm1
    echo 255 > /sys/devices/platform/pwm-fan/hwmon/hwmon2/pwm1
    echo 255 > /sys/kernel/duty_cycle
    echo 255 > /sys/kernel/duty_cycle
    echo 255 > /sys/kernel/duty_cycle
    echo 255 > /usr/bin/fanspeed.conf
    exit 1
elif [ "$fanvall" = 2 ]; then
    echo 192 > /sys/devices/platform/pwm-fan/hwmon/hwmon3/pwm1
    echo 192 > /sys/devices/platform/pwm-fan/hwmon/hwmon1/pwm1
    echo 192 > /sys/devices/platform/pwm-fan/hwmon/hwmon2/pwm1
    echo 192 > /sys/kernel/duty_cycle
    echo 192 > /sys/kernel/duty_cycle
    echo 192 > /sys/kernel/duty_cycle
    echo 192 > /usr/bin/fanspeed.conf
    exit 1
elif [ "$fanvall" = 1 ]; then
    echo 128 > /sys/devices/platform/pwm-fan/hwmon/hwmon3/pwm1
    echo 128 > /sys/devices/platform/pwm-fan/hwmon/hwmon1/pwm1
    echo 128 > /sys/devices/platform/pwm-fan/hwmon/hwmon2/pwm1
    echo 128 > /sys/kernel/duty_cycle
    echo 128 > /sys/kernel/duty_cycle
    echo 128 > /sys/kernel/duty_cycle
    echo 128 > /usr/bin/fanspeed.conf
    exit 1
elif [ "$fanvall" = 0 ]; then
    echo 64 > /sys/devices/platform/pwm-fan/hwmon/hwmon3/pwm1
    echo 64 > /sys/devices/platform/pwm-fan/hwmon/hwmon1/pwm1
    echo 64 > /sys/devices/platform/pwm-fan/hwmon/hwmon2/pwm1
    echo 64 > /sys/kernel/duty_cycle
    echo 64 > /sys/kernel/duty_cycle
    echo 64 > /sys/kernel/duty_cycle
    echo 64 > /usr/bin/fanspeed.conf
    exit 1
fi

while true; do
    fandiver=$(uci get mwrt.@ndis[0].fan_driver 2>/dev/null)
    if [ "$fandiver" != "pwm" ]; then
        fan_gpio=$(uci get mwrt.@ndis[0].fan_gpio 2>/dev/null)
        fan_freq=$(uci get mwrt.@ndis[0].fan_freq 2>/dev/null)
        if lsmod | grep -E -q "Airpi[_-]gpio[_-]fan"; then
            echo ""
        else              
            # 先卸载
            rmmod Airpi-gpio-fan 2>/dev/null
            rmmod Airpi_gpio_fan 2>/dev/null
            # 加载模块
            insmod Airpi-gpio-fan.ko fangpio=$fan_gpio cycle=255 period=$fan_freq fanen=1
        fi

    elif lsmod | grep -E -q "Airpi[_-]gpio[_-]fan"; then
        rmmod Airpi-gpio-fan 2>/dev/null
        rmmod Airpi_gpio_fan 2>/dev/null
    fi

    fanvall_conf=$(cat /etc/fanvallv.conf)
    if [ "$fanvall_conf" = "模组温度" ]; then
        if lsusb | grep -q 5700; then
            temp=$(sendat 1 'AT^CHIPTEMP?' | grep 'CHIPTEMP' | sed -n '1p' | cut -d, -f9 | sed '/^$/d')
            temp=$((temp * 100))
        else
            temp=$(cat /sys/class/thermal/thermal_zone0/temp)
        fi
    else
        temp=$(cat /sys/class/thermal/thermal_zone0/temp)
    fi


    if [ "$temp" -gt 85000 ]; then
        echo 255 > /sys/devices/platform/pwm-fan/hwmon/hwmon3/pwm1
        echo 255 > /sys/devices/platform/pwm-fan/hwmon/hwmon1/pwm1
        echo 255 > /sys/devices/platform/pwm-fan/hwmon/hwmon2/pwm1
        echo 255 > /sys/kernel/duty_cycle
	    echo 255 > /usr/bin/fanspeed.conf
    elif [ "$temp" -gt 60000 ] && [ "$temp" -le 85000 ]; then
        echo 192 > /sys/devices/platform/pwm-fan/hwmon/hwmon3/pwm1
        echo 192 > /sys/devices/platform/pwm-fan/hwmon/hwmon1/pwm1
        echo 192 > /sys/devices/platform/pwm-fan/hwmon/hwmon2/pwm1
        echo 192 > /sys/kernel/duty_cycle
	    echo 192 > /usr/bin/fanspeed.conf
    elif [ "$temp" -gt 50000 ] && [ "$temp" -le 60000 ]; then
        echo 128 > /sys/devices/platform/pwm-fan/hwmon/hwmon3/pwm1
        echo 128 > /sys/devices/platform/pwm-fan/hwmon/hwmon1/pwm1
        echo 128 > /sys/devices/platform/pwm-fan/hwmon/hwmon2/pwm1
        echo 128 > /sys/kernel/duty_cycle
	    echo 128 > /usr/bin/fanspeed.conf
    elif [ "$temp" -gt 0 ] && [ "$temp" -le 50000 ]; then
        echo 64 > /sys/devices/platform/pwm-fan/hwmon/hwmon3/pwm1
        echo 64 > /sys/devices/platform/pwm-fan/hwmon/hwmon1/pwm1
        echo 64 > /sys/devices/platform/pwm-fan/hwmon/hwmon2/pwm1
        echo 64 > /sys/kernel/duty_cycle
	    echo 64 > /usr/bin/fanspeed.conf
    fi
    sleep 8
done


