#!/bin/sh

myos_c8_wifi_mark_if_new() {
    local root="${1:-}"
    myos_c8_board || return 0
    [ -e "$root/overlay/upper/etc/config/wireless" ] && return 0
    [ -e "$root/overlay/etc/config/wireless" ] && return 0
    [ -d "$root/tmp" ] || return 1
    : > "$root/tmp/myos-c8-factory-wifi-init"
}

myos_c8_wifi_validate_mac() {
    local mac first
    mac="$(printf '%s' "$1" | tr 'a-f' 'A-F')"
    case "$mac" in
        [0-9A-F][0-9A-F]:[0-9A-F][0-9A-F]:[0-9A-F][0-9A-F]:[0-9A-F][0-9A-F]:[0-9A-F][0-9A-F]:[0-9A-F][0-9A-F]) ;;
        *) return 1 ;;
    esac
    case "$mac" in
        00:00:00:00:00:00|FF:FF:FF:FF:FF:FF|00:66:88:00:00:00) return 1 ;;
    esac
    first="${mac%%:*}"
    [ $((0x$first & 1)) -eq 0 ] || return 1
    printf '%s\n' "$mac"
}

myos_c8_wifi_mac() {
    local mac
    mac="$(/usr/libexec/myos-c8-identity factory-mac)" || return 1
    myos_c8_wifi_validate_mac "$mac"
}

myos_c8_wifi_is_default() {
    case "$1" in
        NRadio_2.4G|NRadio_5G|OpenWrt|ImmortalWrt|MYOS|\
        NRadio_C5800-688_2.4G|NRadio_C5800-688_5G|\
        NRadio_C8-688_2.4G|NRadio_C8-688_5G|\
        NRadio_NBCPE-688_2.4G|NRadio_NBCPE-688_5G|\
        "MYOS-C8-2.4G-$myos_fw_tail6"|"MYOS-C8-5G-$myos_fw_tail6") return 0 ;;
        *) return 1 ;;
    esac
}

myos_c8_wifi_collect() {
    local section="$1" mode guest is_guest networks network device band hwmode ssid lan=0
    config_get mode "$section" mode
    [ "$mode" = ap ] || return 0
    case "$section" in *guest*) return 0 ;; esac
    config_get guest "$section" guest 0
    config_get is_guest "$section" is_guest 0
    [ "$guest" = 1 ] && return 0
    [ "$is_guest" = 1 ] && return 0
    config_get networks "$section" network
    for network in $networks; do [ "$network" = lan ] && lan=1; done
    [ "$lan" = 1 ] || return 0
    config_get device "$section" device
    [ -n "$device" ] || return 0
    case " $myos_fw_seen " in *" $device "*) return 0 ;; esac
    myos_fw_seen="$myos_fw_seen $device"
    config_get band "$device" band
    case "$band" in
        2g|5g) ;;
        *)
            config_get hwmode "$device" hwmode
            case "$hwmode" in 11a) band=5g ;; 11b|11g) band=2g ;; *) return 0 ;; esac ;;
    esac
    config_get ssid "$section" ssid
    [ "$myos_fw_fresh" = 1 ] || myos_c8_wifi_is_default "$ssid" || return 0
    myos_fw_plan="$myos_fw_plan $section:$band"
    case "$band" in 2g) myos_fw_has2g=1 ;; 5g) myos_fw_has5g=1 ;; esac
}

myos_c8_wifi_apply() {
    local root="${1:-}" mac compact suffix password item section band key
    local myos_fw_fresh=0 myos_fw_seen='' myos_fw_plan='' myos_fw_has2g=0 myos_fw_has5g=0 myos_fw_tail6
    myos_c8_board || return 0
    [ ! -f "$root/tmp/myos-c8-factory-wifi-init" ] || myos_fw_fresh=1
    mac="$(myos_c8_wifi_mac)" || return 1
    compact="$(printf '%s' "$mac" | tr -d ':')"
    suffix="$(printf '%s' "$compact" | cut -c 9-)"
    password=12345678
    myos_fw_tail6="$(printf '%s' "$compact" | cut -c 7-)"
    command -v config_load >/dev/null 2>&1 || . /lib/functions.sh
    config_load wireless || return 1
    config_foreach myos_c8_wifi_collect wifi-iface
    if [ "$myos_fw_fresh" = 1 ]; then
        [ "$myos_fw_has2g" = 1 ] && [ "$myos_fw_has5g" = 1 ] || return 1
    fi
    [ -n "$myos_fw_plan" ] || return 0
    for item in $myos_fw_plan; do
        section="${item%:*}"
        band="${item#*:}"
        case "$band" in 2g) band=2.4G ;; 5g) band=5G ;; esac
        uci -q set "wireless.$section.ssid=@NRadio-$suffix-$band" || return 1
        config_get key "$section" key
        # Retained installations can have a custom password on a factory SSID.
        if [ "$myos_fw_fresh" = 1 ] || [ -z "$key" ]; then
            uci -q set "wireless.$section.key=$password" || return 1
            uci -q set "wireless.$section.encryption=psk2" || return 1
        fi
    done
    uci -q commit wireless || return 1
    [ "$myos_fw_fresh" != 1 ] || rm -f "$root/tmp/myos-c8-factory-wifi-init"
    return 0
}
