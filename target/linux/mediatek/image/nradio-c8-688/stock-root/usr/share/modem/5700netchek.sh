#!/bin/sh

LOCK_FILE="/var/run/modem_network_check.lock"
LOG_FILE="/var/log/modem_network_check.log"
printMsg() {
    local msg="$1"
    logger -t "${PROGRAM}" "${msg}"
} #日志输出调用API

uci_load_network() {
    enable_weburl=$(uci -q get modem.@ndis[0].enable_weburl)
    if [ -z "$enable_weburl" ]; then
        enable_weburl=0
    fi

    weburl=$(uci -q get modem.@ndis[0].weburl)
    if [ -z "$weburl" ]; then
        weburl="www.baidu.com"
    fi

    enable_reconnect=$(uci -q get modem.@ndis[0].enable_reconnect)
    if [ -z "$enable_reconnect" ]; then
        enable_reconnect=0
    fi

    check_time=$(uci -q get modem.@ndis[0].check_time)
    if [ -z "$check_time" ]; then
        check_time="01:00"
    fi
}


network_check() {
    local failure_count=0
    while true; do
        uci_load_network
        if [ "$enable_weburl" = "1" ]; then
            if ping -c 1 $weburl > /dev/null 2>&1; then
                #echo "Network is OK" 
                failure_count=0
            else
                #echo "Network check failed" 
                printMsg "健康检查失败，无网络连接！"
                failure_count=$(($failure_count + 1))
            fi

            if [ $failure_count -ge 6 ]; then
            	printMsg "健康检查失败6次，模组尝试重新拨号..."
            	echo "检测到模组网络掉线检测超过6次，正在尝试重新拨号，请稍后查看网络状态...
		20
		#ff0000" > /tmp/lucimsg.file
                #echo "Network failed 6 times, executing recovery script" 
                /usr/share/modem/mt5700m.sh
                #/sbin/ifup wan
                /sbin/ifup wan6
                failure_count=0
            fi
            sleep 10
        else
           #echo "健康检查关闭，无需检查。" 
            sleep 60  # 添加休眠时间以避免过快循环当 enable_weburl 不为 1
        fi
    done
}


time_based_reconnect() {
    while true; do
        uci_load_network
        if [ "$enable_reconnect" = "1" ]; then
            current_time=$(date +%s) # 获取当前时间的秒数
            scheduled_time=$(date -d "$check_time" +%s) # 获取设定时间的秒数
            let "time_diff = $current_time - $scheduled_time" # 计算时间差

            if [ $time_diff -ge -300 ] && [ $time_diff -le 300 ]; then
            	echo "定时重拨时间到了，模组尝试重新拨号，请稍后查看网络状态...
		20
		#ff0000" > /tmp/lucimsg.file
            	printMsg "定时重拨时间到了，模组尝试重新拨号..."
                #echo "Time to reconnect..." 
                /usr/share/modem/mt5700m.sh
                #/sbin/ifup wan
                /sbin/ifup wan6
                sleep 600 # 休眠10分钟，防止在时间窗口内多次执行
            else
                sleep 30
                #echo "Not time yet. Current time: $(date +%H:%M), Scheduled time: $check_time."
            fi
        else
            #echo "定时重拨关闭，无需检查。" 
            sleep 60  # 添加休眠时间以避免过快循环当 enable_reconnect 不为 1
        fi
    done
}


initialize_uci_defaults() {
    #uci set modem.@ndis[0]=network_check  # 确保节存在

    # 确保每个参数存在并设置默认值
    uci -q get modem.@ndis[0].enable_weburl > /dev/null || uci set modem.@ndis[0].enable_weburl=1
    uci -q get modem.@ndis[0].weburl > /dev/null || uci set modem.@ndis[0].weburl="www.baidu.com"
    uci -q get modem.@ndis[0].enable_reconnect > /dev/null || uci set modem.@ndis[0].enable_reconnect=0
    uci -q get modem.@ndis[0].check_time > /dev/null || uci set modem.@ndis[0].check_time="01:00"

    uci commit modem  # 提交更改，确保值被保存
}

# 在主逻辑开始时调用初始化函数，确保UCI配置存在并设置默认值
initialize_uci_defaults


# 防止脚本重复运行
if [ -e $LOCK_FILE ]; then
    echo "Script is already running." 
    exit
else
    touch $LOCK_FILE
    trap 'rm -f "${LOCK_FILE}"; exit' INT TERM EXIT
    initialize_uci_defaults
    sleep 2
    uci_load_network
    network_check &
    time_based_reconnect &
    #if [ "$enable_weburl" = "1" ]; then
        #network_check &
    #fi

    #if [ "$enable_reconnect" = "1" ]; then
        #time_based_reconnect &
    #fi

    wait
    rm $LOCK_FILE
fi
