#!/bin/sh

myos_c8_board() {
    case "$(cat /tmp/sysinfo/board_name 2>/dev/null)" in
        HCMT7981-688-emmc|nradio,c8-688|HCMT7981-EMMC) return 0 ;;
        *) return 1 ;;
    esac
}

myos_c8_identity() {
    /usr/libexec/myos-c8-identity "$@" 2>/dev/null
}

myos_c8_cloud_identity_apply() {
    myos_c8_board || return 0
    local cfg mac id code password old_id enabled=0 username address
    for cfg in oem cloudd mqttagent report_proactively mosquitto myos_nros; do
        [ -f "/etc/config/$cfg" ] || : > "/etc/config/$cfg"
    done
    mac="$(myos_c8_identity mac)"
    id="$(printf '%s' "$mac" | tr -d ':')"
    code="$(myos_c8_identity code)"
    old_id="$(uci -q get mosquitto.bridge.remote_clientid)"
    uci -q set oem.board=board
    uci -q set "oem.board.id=$mac"
    uci -q set "oem.board.device_code=$code"
    uci -q set oem.board.name=HCMT7981-688-emmc
    uci -q set oem.board.pname=C8-688
    uci -q set oem.board.ptype=rt
    uci -q set oem.board.vendor=nradio
    uci -q set oem.feature=feature
    uci -q set oem.feature.cpe=1
    uci -q commit oem
    [ -n "$id" ] && [ -n "$code" ] && enabled=1
    uci -q get myos_nros.main >/dev/null || uci -q set myos_nros.main=cloud
    uci -q get myos_nros.main.local_enabled >/dev/null || uci -q set myos_nros.main.local_enabled=1
    uci -q get myos_nros.main.remote_web_enabled >/dev/null || uci -q set myos_nros.main.remote_web_enabled=0
    if [ -z "$id" ]; then
        uci -q set myos_nros.main.enabled=0
        uci -q delete mosquitto.bridge
    elif [ "$old_id" != "$id" ]; then
        username=router
        address="$id.dev.nradio.com.cn"
        password="$(myos_c8_identity get remote_password)"
        case "$password" in *[!0-9A-Fa-f]*|'') password='' ;; esac
        if [ "${#password}" = 32 ]; then
            username="$id"
            address="$id.pro.nradio.com.cn"
        else password=''; fi
        uci -q delete mosquitto.bridge
        uci -q set mosquitto.bridge=bridge
        uci -q set "mosquitto.bridge.connection=$id"
        uci -q set "mosquitto.bridge.remote_clientid=$id"
        uci -q set "mosquitto.bridge.remote_username=$username"
        uci -q set "mosquitto.bridge.address=$address"
        [ -z "$password" ] || uci -q set "mosquitto.bridge.remote_password=$password"
        uci -q set mosquitto.bridge.keepalive_interval=10
        uci -q set mosquitto.bridge.bridge_protocol_version=mqttv311
        uci -q set mosquitto.bridge.cleansession=0
        uci -q set mosquitto.bridge.start_type=automatic
        uci -q set mosquitto.bridge.notifications=1
        uci -q set mosquitto.bridge.notifications_local_only=1
        uci -q set mosquitto.bridge.restart_timeout=5
        uci -q set mosquitto.bridge.try_private=0
        uci -q add_list "mosquitto.bridge.topic=sysinfo/# out 1 \"\" $id/"
        uci -q add_list "mosquitto.bridge.topic=exec/# both 1 rpc/ $id/"
        uci -q add_list "mosquitto.bridge.topic=kp/mosca/$id/# out 1 \"\" \"\""
        uci -q add_list "mosquitto.bridge.topic=kp/$id/+/# in 1 \"\" \"\""
        uci -q add_list "mosquitto.bridge.topic=mosca/# in 1 kp/$id/ kp/FFFFFFFFFFFF/"
        uci -q set "myos_nros.main.enabled=$enabled"
    fi
    [ "$enabled" = 1 ] || uci -q set myos_nros.main.enabled=0
    uci -q set mosquitto.owrt=owrt
    uci -q set mosquitto.owrt.use_uci=1
    uci -q set mosquitto.mosquitto=mosquitto
    uci -q set mosquitto.mosquitto.log_dest=syslog
    uci -q set mosquitto.mosquitto.log_types=warning
    uci -q set mosquitto.listener=listener
    uci -q set mosquitto.listener.port=1883
    uci -q set mosquitto.listener.no_remote_access=1
    uci -q set cloudd.config=cloudd
    uci -q set cloudd.config.version=1.0
    uci -q set mqttagent.config=mqttagent
    uci -q set mqttagent.config.sub_mosca=0
    uci -q set report_proactively.config=config
    uci -q set report_proactively.config.interval=2
    for cfg in myos_nros mosquitto cloudd mqttagent report_proactively; do uci -q commit "$cfg"; done
}
