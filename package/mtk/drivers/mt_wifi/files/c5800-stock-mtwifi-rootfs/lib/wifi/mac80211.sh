# this file will be included in
#     /lib/wifi/{phyname}.sh

scan_mac80211() {
    return 0
}

disable_mac80211() {
    logger -t "wifi" "disable $@"
    local device="$1"
    set_wifi_down "$device"
}

enable_mac80211() {
    logger -t "wifi" "enable $@"
    local device="$1"
    config_get vifs "$device" vifs

    # bring up vifs
    for vif in $vifs; do
        config_get_bool disabled "$vif" disabled 0
        [ $disabled -eq 0 ] || continue

        config_get ifname "$vif" ifname

        local net_cfg bridge
        net_cfg="$(find_net_config "$vif")"
        [ -z "$net_cfg" ] || {
            bridge="$(bridge_interface "$net_cfg")"
            config_set "$vif" bridge "$bridge"
            start_net "$ifname" "$net_cfg"
            logger -t "wifi" "start_net $ifname $net_cfg"
        }

        set_wifi_up "$vif" "$ifname"
    done
}
