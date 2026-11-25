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

myos_c8_mqtt_password_valid() {
    case "$1" in *[!0-9A-Fa-f]*) return 1 ;; esac
    [ "${#1}" -eq 32 ]
}

myos_c8_stock_cloud_password() (
    local id="$1" device offset directory loop config mac password
    device="$(/usr/sbin/blkid -t PARTLABEL=rootfs -o device)" || return 1
    [ -b "$device" ] || return 1
    # The factory F2FS overlay follows the 64 KiB-aligned SquashFS image.
    offset="$(MYOS_C8_STOCK_ROOTFS="$device" lua -e '
        local f = io.open(os.getenv("MYOS_C8_STOCK_ROOTFS"), "rb")
        if not f or f:read(4) ~= "hsqs" or not f:seek("set", 40) then os.exit(1) end
        local size = f:read(8)
        if not size or #size ~= 8 then os.exit(1) end
        local bytes = 0
        for i = 8, 1, -1 do bytes = bytes * 256 + size:byte(i) end
        local offset = math.ceil(bytes / 65536) * 65536
        if not f:seek("set", offset + 1024) or f:read(4) ~= string.char(16,32,245,242) then os.exit(1) end
        print(offset)
    ')" || return 1
    directory="$(mktemp -d /tmp/myos-c8-stock.XXXXXX)" || return 1
    trap '
        if [ -n "$loop" ]; then
            umount "$directory" 2>/dev/null
            losetup -d "$loop"
        fi
        rmdir "$directory"
    ' EXIT
    loop="$(losetup --find --show --read-only --offset "$offset" "$device")" || return 1
    mount -t f2fs -o ro,norecovery,nodev,nosuid,noexec "$loop" "$directory" || return 1
    config="$directory/upper/etc/config"
    mac="$(uci -q -c "$config" get oem.board.id | tr -d : | tr a-f A-F)"
    [ "$mac" = "$id" ] || return 1
    [ "$(uci -q -c "$config" get mosquitto.bridge.remote_username)" = "$id" ] || return 1
    [ "$(uci -q -c "$config" get mosquitto.bridge.address)" = "$id.pro.nradio.com.cn" ] || return 1
    password="$(uci -q -c "$config" get mosquitto.bridge.remote_password)"
    myos_c8_mqtt_password_valid "$password" || return 1
    printf '%s\n' "$password"
)

myos_c8_restore_cloud_credentials() {
    local id="$1" address password
    address="$(uci -q get mosquitto.bridge.address)"
    case "$address" in "$id.dev.nradio.com.cn"|"$id.pro.nradio.com.cn") ;; *) return 0 ;; esac
    password="$(uci -q get mosquitto.bridge.remote_password)"
    myos_c8_mqtt_password_valid "$password" && return 0
    password="$(myos_c8_identity get remote_password)"
    if ! myos_c8_mqtt_password_valid "$password"; then
        password="$(myos_c8_stock_cloud_password "$id")" || return 0
    fi
    printf 'set mosquitto.bridge.remote_username="%s"\nset mosquitto.bridge.remote_password="%s"\nset mosquitto.bridge.address="%s.pro.nradio.com.cn"\n' \
        "$id" "$password" "$id" | uci -q batch
}

myos_c8_cloud_identity_apply() {
    myos_c8_board || return 0
    local cfg mac id code old_id enabled=0
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
        uci -q delete mosquitto.bridge
        uci -q set mosquitto.bridge=bridge
        uci -q set "mosquitto.bridge.connection=$id"
        uci -q set "mosquitto.bridge.remote_clientid=$id"
        uci -q set mosquitto.bridge.remote_username=router
        uci -q set "mosquitto.bridge.address=$id.dev.nradio.com.cn"
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
    [ -z "$id" ] || myos_c8_restore_cloud_credentials "$id"
    [ "$enabled" = 1 ] || uci -q set myos_nros.main.enabled=0
    uci -q set mosquitto.owrt=owrt
    uci -q set mosquitto.owrt.use_uci=1
    uci -q set mosquitto.mosquitto=mosquitto
    uci -q set mosquitto.mosquitto.log_dest=syslog
    uci -q set mosquitto.mosquitto.log_types=warning
    uci -q get mosquitto.mosquitto.allow_anonymous >/dev/null || uci -q set mosquitto.mosquitto.allow_anonymous=1
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
