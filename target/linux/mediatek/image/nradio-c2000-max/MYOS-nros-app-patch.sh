#!/bin/sh
set -eu

root="${1:?rootfs path required}"
script_dir="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"

[ -d "$root" ] || { echo "C2000MAX NROS app patch missing rootfs: $root"; exit 1; }

# Free enough compressed space for NROS APP bridge without removing user-facing plugins.
rm -rf "$root/usr/share/hwdata"

mkdir -p "$root/etc/config" "$root/etc/init.d" "$root/etc/uci-defaults" "$root/etc/rc.d" "$root/usr/sbin" "$root/etc/nradio"
touch "$root/etc/config/oem" "$root/etc/config/cloudd" "$root/etc/config/mqttagent" \
	"$root/etc/config/report_proactively" "$root/etc/config/mosquitto" "$root/etc/config/myos_nros"

cat > "$root/etc/config/myos_nros" <<'EOF'
config cloud 'main'
	option local_enabled '1'
	option enabled '1'
	option remote_web_enabled '1'
	option remote_ssh_enabled '1'
	option remote_access_version '2'
EOF

# Shared images must not carry a device identity. Per-device builds must opt in
# with MYOS_NROS_IDENTITY_FILE; runtime still prefers bdinfo.
identity_seed="${MYOS_NROS_IDENTITY_FILE:-}"
rm -f "$root/etc/nradio/identity" "$root/boot/nradio-identity" 2>/dev/null || true
if [ -n "$identity_seed" ]; then
	[ -f "$identity_seed" ] || { echo "C2000MAX missing MYOS_NROS_IDENTITY_FILE: $identity_seed"; exit 1; }
	cp -fp "$identity_seed" "$root/etc/nradio/identity"
fi

mkdir -p "$root/usr/lib/lua/luci/controller/nradio" "$root/usr/lib/lua/luci/view/myos_nros"
cp -fp "$script_dir/myos-nradio-app.lua" "$root/usr/lib/lua/luci/controller/nradio/app.lua"
cp -fp "$script_dir/myos_nradio_app_crypto.lua" "$root/usr/lib/lua/myos_nradio_app_crypto.lua"
cp -fp "$script_dir/myos_nradio_app_session.lua" "$root/usr/lib/lua/myos_nradio_app_session.lua"
chmod 644 "$root/usr/lib/lua/myos_nradio_app_session.lua"
cp -fp "$script_dir/myos_nradio_data.lua" "$root/usr/lib/lua/myos_nradio_data.lua"
cp -fp "$script_dir/plmn.json" "$root/etc/plmn.json"
cp -fp "$script_dir/myos-nradio-app-fast.lua" "$root/usr/sbin/myos-nradio-app-fast"
cp -fp "$script_dir/myos-nradio-discovery.lua" "$root/usr/sbin/myos-nradio-discovery"
mkdir -p "$root/usr/lib/lua/luci/controller" "$root/usr/lib/lua/luci/model/cbi"
cp -fp "$script_dir/myos-nros-cloud-controller.lua" "$root/usr/lib/lua/luci/controller/myos_nros.lua"
cp -fp "$script_dir/myos-nros-cloud-cbi.lua" "$root/usr/lib/lua/luci/model/cbi/myos_nros.lua"
cp -fp "$script_dir/myos-nros-cloud-status.htm" "$root/usr/lib/lua/luci/view/myos_nros/status.htm"

# Keep the runtime helpers as reviewable files shared with live validation.
[ -d "$script_dir/myos-remote-files" ] || { echo "Missing remote runtime files"; exit 1; }
cp -a "$script_dir/myos-remote-files/." "$root/"
chmod 755 "$root/etc/init.d/myos-nros-ssh" "$root/etc/uci-defaults/30-myos-nros-remote-access"

cat > "$root/etc/init.d/myos-nros-app" <<'EOF'
#!/bin/sh /etc/rc.common
START=32
STOP=68
USE_PROCD=1

cloud_enabled() {
	[ "$(uci -q get myos_nros.main.enabled 2>/dev/null)" = "1" ]
}

local_enabled() {
	[ "$(uci -q get myos_nros.main.local_enabled 2>/dev/null)" != "0" ]
}

start_service() {
	local pid
	if local_enabled; then
		procd_open_instance discovery
		procd_set_param command /usr/sbin/myos-nradio-discovery
		procd_set_param respawn 3600 5 5
		procd_close_instance
	fi
	cloud_enabled || return 0
	for pid in $(pgrep -f '/usr/sbin/myos-nros-app-agent' 2>/dev/null); do
		[ "$pid" = "$$" ] || kill "$pid" 2>/dev/null || true
	done
	ubus -t 20 wait_for network.interface.loopback >/dev/null 2>&1 || true
	procd_open_instance command
	procd_set_param command /usr/sbin/myos-nros-app-agent command
	procd_set_param respawn 5 5 -1
	procd_close_instance
	procd_open_instance status
	procd_set_param command /usr/sbin/myos-nros-app-agent status
	procd_set_param respawn 5 5 -1
	procd_close_instance
}

stop_service() {
	local pid
	for pid in $(pgrep -f '/usr/sbin/myos-nros-app-agent' 2>/dev/null); do
		[ "$pid" = "$$" ] || kill "$pid" 2>/dev/null || true
	done
}

EOF

# usr/sbin/myos-nros-cloudctl is supplied by myos-remote-files above.

# etc/uci-defaults/29-myos-nros-app is supplied by myos-remote-files above.

# usr/sbin/myos-nros-app-agent is supplied by myos-remote-files above.

chmod 755 "$root/etc/init.d/myos-nros-mqtt" "$root/etc/init.d/myos-nros-app" \
	"$root/etc/uci-defaults/29-myos-nros-app" "$root/usr/sbin/myos-nros-app-agent" \
	"$root/usr/sbin/myos-nros-cloudctl" "$root/usr/sbin/myos-nradio-app-fast" \
	"$root/usr/sbin/myos-nradio-discovery" "$root/usr/sbin/mosquitto" \
	"$root/usr/lib/lua/mosquitto.so" "$root/usr/lib/libmosquitto.so.1"
chmod 755 "$root/usr/bin/mwrt-ledsched" "$root/etc/init.d/mwrt-ledsched" \
	"$root/etc/uci-defaults/99e-mwrt-ledsched" \
	"$root/usr/bin/sendat" "$root/usr/bin/pdu_decoder" "$root/usr/bin/smssend"
chmod 644 "$root/usr/lib/lua/luci/controller/myos_nros.lua" \
	"$root/usr/lib/lua/luci/model/cbi/myos_nros.lua" \
	"$root/usr/lib/lua/luci/view/myos_nros/status.htm" \
	"$root/usr/lib/lua/myos_nradio_data.lua"

mkdir -p "$root/www/cgi-bin"
if [ -f "$root/www/cgi-bin/luci" ] && [ ! -f "$root/www/cgi-bin/luci.ucode" ]; then
	mv "$root/www/cgi-bin/luci" "$root/www/cgi-bin/luci.ucode"
fi
cat > "$root/www/cgi-bin/luci" <<'EOF'
#!/bin/sh
local_enabled() {
	[ "$(uci -q get myos_nros.main.local_enabled 2>/dev/null)" != "0" ]
}

case "${PATH_INFO:-}" in
	/nradio/app|/nradio/app/*)
		if local_enabled; then
			exec /usr/bin/lua /usr/sbin/myos-nradio-app-fast
		fi
		printf 'Status: 404 Not Found\r\nContent-Type: application/json\r\n\r\n{"code":"1","result":"local app disabled"}'
		;;
	*) exec /www/cgi-bin/luci.ucode ;;
esac
EOF
chmod 755 "$root/www/cgi-bin/luci"
rm -f "$root"/etc/rc.d/*mosquitto* "$root"/etc/rc.d/*myos-nros-app*
# First boot enables cloud access when a valid factory identity is available.
rm -f "$root"/etc/rc.d/*myos-nros-mqtt*
ln -sf ../init.d/myos-nros-mqtt "$root/etc/rc.d/S30myos-nros-mqtt"
ln -sf ../init.d/myos-nros-mqtt "$root/etc/rc.d/K70myos-nros-mqtt"
ln -sf ../init.d/myos-nros-app "$root/etc/rc.d/S32myos-nros-app"
ln -sf ../init.d/myos-nros-app "$root/etc/rc.d/K68myos-nros-app"
ln -sf ../init.d/quickstart "$root/etc/rc.d/S92quickstart"

if [ -f "$root/etc/config/uhttpd" ] &&
	! grep -q '0\.0\.0\.0:82' "$root/etc/config/uhttpd"; then
	sed -i '/list listen_http[[:space:]]*0\.0\.0\.0:80/a\	list listen_http\t0.0.0.0:82' \
		"$root/etc/config/uhttpd"
fi

python3 "$script_dir/../../../../../scripts/prepare-public-rootfs.py" "$root"

exit 0
