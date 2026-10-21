#!/bin/sh

lock_file="/var/run/network_status_update.lock"
exec 200>"$lock_file"
flock -n 200 || exit 1

LED_DIR="/sys/class/leds"
STATE_FILE="/tmp/netmodeled-led-state"
MANUAL_FLAG="/tmp/ledflag.conf"
HOLD_MISSES=10

led_write() {
    led="$LED_DIR/hc:blue:$1"
    [ -d "$led" ] || return

    if [ "$2" = "0" ]; then
        value=0
    else
        value=$(cat "$led/max_brightness" 2>/dev/null)
        case "$value" in
            ''|*[!0-9]*|0) value=255 ;;
        esac
    fi

    [ -w "$led/trigger" ] && echo none > "$led/trigger" 2>/dev/null
    [ -w "$led/brightness" ] && echo "$value" > "$led/brightness"
}

read_state() {
    LED_MODE="5g"
    LED_BARS="3"
    LED_MISSES=0

    if [ -f "$STATE_FILE" ]; then
        read saved_mode saved_bars saved_misses < "$STATE_FILE"
        case "$saved_mode" in
            5g|4g) LED_MODE="$saved_mode" ;;
        esac
        case "$saved_bars" in
            1|2|3) LED_BARS="$saved_bars" ;;
        esac
        case "$saved_misses" in
            ''|*[!0-9]*) LED_MISSES=0 ;;
            *) LED_MISSES="$saved_misses" ;;
        esac
    fi
}

save_state() {
    echo "$LED_MODE $LED_BARS ${LED_MISSES:-0}" > "$STATE_FILE"
}

iface_has_ipv4() {
    ip -4 addr show dev "$1" 2>/dev/null | grep -q 'inet '
}

iface_has_ipv6_default() {
    ip -6 route show default dev "$1" 2>/dev/null | grep -q .
}

iface_has_ipv4_default() {
    awk -v dev="$1" '$1 == dev && $2 == "00000000" { found = 1 } END { exit found ? 0 : 1 }' /proc/net/route
}

internet_online() {
    iface_has_ipv4_default BLUE4 && iface_has_ipv4 BLUE4 && return 0
    iface_has_ipv4_default eth1 && iface_has_ipv4 eth1 && return 0
    iface_has_ipv6_default BLUE4 && return 0
    iface_has_ipv6_default eth1 && return 0
    return 1
}

blue4_online() {
    iface_has_ipv4_default BLUE4 && iface_has_ipv4 BLUE4 && return 0
    iface_has_ipv6_default BLUE4 && return 0
    return 1
}

blue4_gateway_hex() {
    awk '$1 == "BLUE4" && $2 == "00000000" { print $3; exit }' /proc/net/route
}

blue4_type_override() {
    if [ -r /tmp/blue4_led_type ]; then
        cat /tmp/blue4_led_type 2>/dev/null
        return
    fi

    uci -q get modem.@ndis[0].blue4_type 2>/dev/null
}

blue4_forced_broadband() {
    type=$(blue4_type_override)
    case "$type" in
        broadband|wan|internet|fiber|dhcp)
            return 0
            ;;
    esac

    return 1
}

blue4_cellular_hint() {
    type=$(blue4_type_override)
    case "$type" in
        5g|nr|cellular|modem)
            LED_MODE="5g"
            CELLULAR_SEEN=1
            return 0
            ;;
        4g|lte)
            LED_MODE="4g"
            CELLULAR_SEEN=1
            return 0
            ;;
        broadband|wan|internet|fiber|dhcp)
            return 1
            ;;
    esac

    # C5800 BLUE4 5G/ODU path normally receives 10.x/8 with gateway 10.0.0.1.
    if [ "$(blue4_gateway_hex)" = "0100000A" ]; then
        LED_MODE="5g"
        CELLULAR_SEEN=1
        return 0
    fi

    return 1
}

hold_cellular_state() {
    blue4_online || return 1
    blue4_forced_broadband && return 1
    case "$LED_MODE" in
        5g|4g) ;;
        *) return 1 ;;
    esac
    case "$LED_BARS" in
        1|2|3) ;;
        *) return 1 ;;
    esac
    [ "${LED_MISSES:-0}" -lt "$HOLD_MISSES" ] || return 1

    LED_MISSES=$((LED_MISSES + 1))
    CELLULAR_SEEN=1
    return 0
}

apply_internet_only_leds() {
    led_write cmode5 0
    led_write cmode4 0
    led_write sig1 0
    led_write sig2 0
    led_write sig3 0
    led_write int 1
}

bars_from_rsrq() {
    value="${1%%.*}"
    case "$value" in
        ''|*[!0-9-]*) return 1 ;;
    esac

    if [ "$value" -ge -12 ]; then
        echo 3
    elif [ "$value" -ge -17 ]; then
        echo 2
    else
        echo 1
    fi
}

bars_from_csq() {
    value="$1"
    case "$value" in
        ''|*[!0-9]*) return 1 ;;
    esac

    [ "$value" -eq 99 ] && return 1

    if [ "$value" -ge 20 ]; then
        echo 3
    elif [ "$value" -ge 10 ]; then
        echo 2
    else
        echo 1
    fi
}

set_signal_bars() {
    case "$LED_BARS" in
        3)
            led_write sig1 1
            led_write sig2 1
            led_write sig3 1
            ;;
        2)
            led_write sig1 1
            led_write sig2 1
            led_write sig3 0
            ;;
        *)
            LED_BARS="1"
            led_write sig1 1
            led_write sig2 0
            led_write sig3 0
            ;;
    esac
}

set_mode_leds() {
    case "$LED_MODE" in
        4g)
            led_write cmode5 0
            led_write cmode4 1
            ;;
        *)
            LED_MODE="5g"
            led_write cmode5 1
            led_write cmode4 0
            ;;
    esac
}

apply_cellular_leds() {
    set_mode_leds
    set_signal_bars
    save_state
}

apply_connected_leds() {
    apply_cellular_leds
    led_write int 1
}

apply_cellular_offline_leds() {
    apply_cellular_leds
    led_write int 0
}

apply_offline_leds() {
    led_write cmode5 0
    led_write cmode4 0
    led_write sig1 0
    led_write sig2 0
    led_write sig3 0
    led_write int 0
}

detect_rm520n() {
    network_mode=$(sendat 2 at+qnwinfo | grep '+QNWINFO' | head -n 1 | awk -F\" '{print $2}' | tr -d '\r')
    csq_line=$(sendat 2 "AT+CSQ" | grep "+CSQ:")
    csq=$(echo "$csq_line" | grep -o "+CSQ: [0-9]\{1,2\}" | grep -o "[0-9]\{1,2\}")

    case "$network_mode" in
        *5G*|*NR*) LED_MODE="5g"; CELLULAR_SEEN=1 ;;
        *LTE*) LED_MODE="4g"; CELLULAR_SEEN=1 ;;
    esac

    bars=$(bars_from_csq "$csq")
    if [ -n "$bars" ]; then
        LED_BARS="$bars"
        CELLULAR_SEEN=1
    fi
}

detect_mt5700() {
    hcsq=$(sendat 1 'AT^HCSQ?' | grep '\^HCSQ' | head -n 1)
    monsc=$(sendat 1 'AT^MONSC' | grep -E 'NR|LTE' | head -n 1)

    if echo "$monsc" | grep -q 'NR'; then
        LED_MODE="5g"
        CELLULAR_SEEN=1
        rsrq=$(echo "$monsc" | cut -d ',' -f10 | sed 's/[^-.0-9]//g')
        bars=$(bars_from_rsrq "$rsrq")
        [ -n "$bars" ] && LED_BARS="$bars"
        return
    fi

    if echo "$monsc" | grep -q 'LTE'; then
        LED_MODE="4g"
        CELLULAR_SEEN=1
        rsrq=$(echo "$monsc" | cut -d ',' -f9 | sed 's/[^-.0-9]//g')
        bars=$(bars_from_rsrq "$rsrq")
        [ -n "$bars" ] && LED_BARS="$bars"
        return
    fi

    if echo "$hcsq" | grep -Eq 'NR|5G'; then
        LED_MODE="5g"
        CELLULAR_SEEN=1
        return
    fi

    if echo "$hcsq" | grep -q 'LTE'; then
        LED_MODE="4g"
        CELLULAR_SEEN=1
        return
    fi
}

update_leds() {
    read_state
    CELLULAR_SEEN=0
    modconf=$(cat /tmp/modconf.conf 2>/dev/null)

    if echo "$modconf" | grep -q "RM520N"; then
        detect_rm520n
    elif echo "$modconf" | grep -q "MT5700"; then
        detect_mt5700
    fi

    if [ "$CELLULAR_SEEN" != "1" ] && blue4_online; then
        blue4_cellular_hint
    fi

    if [ "$CELLULAR_SEEN" = "1" ]; then
        LED_MISSES=0
    else
        hold_cellular_state
    fi

    if [ "$CELLULAR_SEEN" = "1" ] && internet_online; then
        apply_connected_leds
    elif [ "$CELLULAR_SEEN" = "1" ]; then
        apply_cellular_offline_leds
    elif internet_online; then
        apply_internet_only_leds
    else
        apply_offline_leds
    fi
}

while true
do
    if [ ! -f "$MANUAL_FLAG" ]; then
        update_leds
    fi
    sleep 9
done
