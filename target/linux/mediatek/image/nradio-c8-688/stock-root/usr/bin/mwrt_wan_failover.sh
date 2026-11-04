#!/bin/sh
# Mwrt WAN Failover - 优化日志输出

CONFIG_FILE="Mwrt-Assurance"
CONFIG_SECTION="wan_failover"
LOG_FILE="/tmp/mwrt_failover.log"

mkdir -p /tmp
[ ! -f "$LOG_FILE" ] && touch "$LOG_FILE"

########################################
# 日志等级控制（DEBUG/INFO/WARN/ERROR）
LOG_LEVEL="INFO"   # 默认只记录 INFO 及以上
MAX_LOG_SIZE=10240 # KB，日志轮替阈值

should_log() {
    local level="$1"
    case "$LOG_LEVEL" in
        DEBUG) return 0 ;;  # 全部记录
        INFO)  [ "$level" != "DEBUG" ] && return 0 || return 1 ;;
        WARN)  [ "$level" = "WARN" ] || [ "$level" = "ERROR" ] && return 0 || return 1 ;;
        ERROR) [ "$level" = "ERROR" ] && return 0 || return 1 ;;
        *) return 1 ;;
    esac
}

rotate_log() {
    if [ -f "$LOG_FILE" ] && [ "$(du -k "$LOG_FILE" | cut -f1)" -ge "$MAX_LOG_SIZE" ]; then
        mv "$LOG_FILE" "$LOG_FILE.old"
        touch "$LOG_FILE"
    fi
}

log_msg() {
    local level="$1"; shift
    local msg="$*"
    should_log "$level" || return 0
    rotate_log
    local ts; ts=$(date '+%F %T')
    echo "$ts [$level] $msg" >> "$LOG_FILE"
}

# 状态变化才记录
last_state=""
set_state() {
    local new_state="$1"
    if [ "$last_state" != "$new_state" ]; then
        log_msg "INFO" "WAN 状态变化: $new_state"
        last_state="$new_state"
    fi
}

########################################
get_config_value() {
    local option="$1"; local default="$2"
    local value
    value=$(uci get ${CONFIG_FILE}.${CONFIG_SECTION}.${option} 2>/dev/null)
    if [ -n "$value" ]; then
        echo "$value"
    else
        echo "$default"
    fi
}

get_iface_name_by_dev() {
    local dev="$1"
    uci show network 2>/dev/null | grep "device='$dev'" | awk -F. '{print $2}' | awk -F= '{print $1}' | head -n 1
}

get_gateway() {
    local ifname="$1"
    local gw

    gw=$(ip route show dev "$ifname" | awk '/^default / {print $3; exit}')
    [ -n "$gw" ] && echo "$gw" && return 0

    local netname
    netname=$(get_iface_name_by_dev "$ifname")
    if [ -n "$netname" ]; then
        gw=$(uci get network."$netname".gateway 2>/dev/null)
        [ -n "$gw" ] && echo "$gw" && return 0
    fi

    if [ -f "/tmp/dhcp.leases" ]; then
        gw=$(grep "$ifname" /tmp/dhcp.leases | awk '{print $3}' | head -n 1)
        [ -n "$gw" ] && echo "$gw" && return 0
    fi

    gw=$(ip route | grep "$ifname" | awk '/default/ {print $3; exit}')
    [ -n "$gw" ] && echo "$gw" && return 0

    gw=$(ip addr show dev "$ifname" | awk '/inet / {print $2}' | cut -d/ -f1 | cut -d. -f1-3).1
    [ -n "$gw" ] && echo "$gw" && return 0

    return 1
}

check_primary_via_temp_route() {
    local ifname="$1"
    local target="$2"

    local gw
    gw=$(get_gateway "$ifname") || return 1

    if ! ip addr show dev "$ifname" | grep -q "inet "; then
        return 1
    fi

    ip route add "$target" via "$gw" dev "$ifname" 2>/dev/null
    local result=1
    if ping -I "$ifname" -c 2 -W 2 "$target" >/dev/null 2>&1; then
        result=0
    fi
    ip route del "$target" via "$gw" dev "$ifname" 2>/dev/null

    return $result
}

check_online() {
    local ifname="$1"; local ping_target="$PING_TARGET"

    if ! ip addr show dev "$ifname" | grep -q "inet "; then
        ifup "$ifname"
        sleep 2
    fi

    if ping -I "$ifname" -c 2 -W 2 "$ping_target" >/dev/null 2>&1; then
        return 0
    else
        return 1
    fi
}

switch_to() {
    local ifname="$1"; local gw="$2"
    [ -z "$gw" ] && log_msg "ERROR" "切换失败 $ifname: 无网关" && return
    ip route del default >/dev/null 2>&1
    ip route replace default via "$gw" dev "$ifname"
    CURRENT_IF="$ifname"
    log_msg "INFO" "默认路由已切换到 $ifname ($gw)"
}

########################################
CURRENT_IF=$(ip route | awk '/^default/ {print $5; exit}')
fail_count=0
success_count=0

ENABLED=$(get_config_value "enabled" "1")
PRIMARY_IF=$(get_config_value "primary_if" "BLUE4")
SECONDARY_IF=$(get_config_value "secondary_if" "eth1")
PING_TARGET=$(get_config_value "ping_target" "223.5.5.5")
CHECK_INTERVAL=$(get_config_value "check_interval" "5")
FAIL_THRESHOLD=$(get_config_value "fail_threshold" "2")
SUCCESS_THRESHOLD=$(get_config_value "success_threshold" "3")
FORCE_PRIMARY=$(get_config_value "force_primary" "1")

log_msg "INFO" "脚本启动，当前接口=$CURRENT_IF"

while true; do
    ENABLED=$(uci get ${CONFIG_FILE}.${CONFIG_SECTION}.enabled 2>/dev/null || echo "1")
    PRIMARY_IF=$(uci get ${CONFIG_FILE}.${CONFIG_SECTION}.primary_if 2>/dev/null || echo "BLUE4")
    SECONDARY_IF=$(uci get ${CONFIG_FILE}.${CONFIG_SECTION}.secondary_if 2>/dev/null || echo "eth1")
    PING_TARGET=$(uci get ${CONFIG_FILE}.${CONFIG_SECTION}.ping_target 2>/dev/null || echo "223.5.5.5")
    CHECK_INTERVAL=$(uci get ${CONFIG_FILE}.${CONFIG_SECTION}.check_interval 2>/dev/null || echo "5")
    FAIL_THRESHOLD=$(uci get ${CONFIG_FILE}.${CONFIG_SECTION}.fail_threshold 2>/dev/null || echo "2")
    SUCCESS_THRESHOLD=$(uci get ${CONFIG_FILE}.${CONFIG_SECTION}.success_threshold 2>/dev/null || echo "3")
    FORCE_PRIMARY=$(uci get ${CONFIG_FILE}.${CONFIG_SECTION}.force_primary 2>/dev/null || echo "1")

    [ "$ENABLED" != "1" ] && sleep "$CHECK_INTERVAL" && continue

    if [ "$CURRENT_IF" = "$PRIMARY_IF" ]; then
        if check_online "$PRIMARY_IF"; then
            fail_count=0
            success_count=$((success_count + 1))
            set_state "PRIMARY_OK"
            echo 1 > /sys/class/leds/blue:wlan/brightness
            echo 0 > /sys/class/leds/red:wan/brightness
        else
            success_count=0
            fail_count=$((fail_count + 1))
            if [ "$fail_count" -ge "$FAIL_THRESHOLD" ]; then
                log_msg "WARN" "主WAN故障，准备切换到备用WAN: $SECONDARY_IF"

                if ! check_online "$SECONDARY_IF"; then
                    ifup "$SECONDARY_IF"
                    sleep 3
                fi

                gw_secondary=$(get_gateway "$SECONDARY_IF")
                if [ -n "$gw_secondary" ]; then
                    switch_to "$SECONDARY_IF" "$gw_secondary"
                    set_state "SECONDARY_ACTIVE"
                    echo 1 > /sys/class/leds/blue:wlan/brightness
                    echo 1 > /sys/class/leds/red:wan/brightness
                else
                    log_msg "ERROR" "备用WAN无网关，无法切换"
                    echo 0 > /sys/class/leds/blue:wlan/brightness
                    echo 1 > /sys/class/leds/red:wan/brightness
                fi
                fail_count=0
            fi
        fi
    else
        if [ "$FORCE_PRIMARY" = "1" ]; then
            if check_primary_via_temp_route "$PRIMARY_IF" "$PING_TARGET"; then
                log_msg "INFO" "检测到主WAN恢复，切换路由到主WAN"
                gw_primary=$(get_gateway "$PRIMARY_IF")
                if [ -n "$gw_primary" ]; then
                    switch_to "$PRIMARY_IF" "$gw_primary"
                    set_state "PRIMARY_OK"
                    echo 1 > /sys/class/leds/blue:wlan/brightness
                    echo 0 > /sys/class/leds/red:wan/brightness
                fi
            fi
        fi
    fi
    sleep "$CHECK_INTERVAL"
done
