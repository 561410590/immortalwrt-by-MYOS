#!/bin/sh
set -eu

root="${1:?rootfs path required}"
factory="${2:?factory rootfs path required}"

[ -d "$root" ] || { echo "C2000MAX cloud patch missing rootfs: $root"; exit 1; }
[ -d "$factory" ] || { echo "C2000MAX cloud patch missing factory rootfs: $factory"; exit 1; }

copy_path() {
	local path="$1"
	[ -e "$factory/$path" ] || { echo "C2000MAX missing cloud file: $path"; exit 1; }
	mkdir -p "$root/$(dirname "$path")"
	cp -fpR "$factory/$path" "$root/$path"
}

copy_optional() {
	local path="$1"
	[ -e "$factory/$path" ] || return 0
	mkdir -p "$root/$(dirname "$path")"
	cp -fpR "$factory/$path" "$root/$path"
}

copy_opkg_info() {
	local pkg="$1"
	local src base
	mkdir -p "$root/usr/lib/opkg/info"
	for src in "$factory/usr/lib/opkg/info/$pkg."*; do
		[ -e "$src" ] || continue
		base="$(basename "$src")"
		cp -fp "$src" "$root/usr/lib/opkg/info/$base"
	done
}

append_status() {
	local pkg="$1"
	local status="$root/usr/lib/opkg/status"
	local control="$root/usr/lib/opkg/info/$pkg.control"
	[ -f "$control" ] || return 0
	mkdir -p "$(dirname "$status")"
	[ -f "$status" ] || : > "$status"
	grep -q "^Package: $pkg$" "$status" && return 0
	awk 'BEGIN{has_status=0; has_time=0} /^Status: /{has_status=1} /^Installed-Time: /{has_time=1} {print} END{if (!has_status) print "Status: install ok installed"; if (!has_time) print "Installed-Time: 1772529460"; print ""}' "$control" >> "$status"
}

for path in \
	usr/bin/bdinfo \
	usr/bin/cloudd \
	usr/bin/mqttagent \
	usr/sbin/mosquitto \
	usr/sbin/report_proactively \
	usr/lib/cloudd \
	usr/lib/lua/cloudd \
	usr/lib/lua/luci/nradio.lua \
	usr/lib/lua/luci/model/cloudd.lua \
	etc/init.d/cloudd \
	etc/init.d/mosquitto \
	etc/init.d/mqttagent \
	etc/init.d/report_proactively \
	etc/config/cloudd \
	etc/config/cloudd_acl \
	etc/config/cloudd_cli \
	etc/config/mosquitto \
	etc/config/mqttagent \
	etc/config/report_proactively \
	etc/mosquitto/mosquitto_acl.conf \
	etc/ptype.d/rt/30_mosq; do
	copy_path "$path"
done

for path in \
	lib/libubox.so.20210516 \
	lib/libblobmsg_json.so.20210516 \
	lib/libubus.so.20210630 \
	usr/lib/libjson-c.so.5 \
	usr/lib/libjson-c.so.5.1.0 \
	usr/lib/libcrypto.so.1.1 \
	usr/lib/libssl.so.1.1 \
	usr/lib/libnradio-utils.so \
	usr/lib/libnradio-utils.so.1.0.0 \
	usr/lib/libbdinfo.so \
	usr/lib/libbdinfo.so.3.3 \
	usr/lib/libmosquitto.so \
	usr/lib/libmosquitto.so.1 \
	usr/lib/libcares.so.2 \
	usr/lib/libcares.so.2.4.3 \
	usr/lib/lua/cjson.so \
	usr/lib/lua/md5.lua \
	usr/lib/lua/md5 \
	usr/lib/lua/nixio.so \
	usr/lib/lua/nixio \
	usr/lib/lua/socket.lua \
	usr/lib/lua/socket \
	usr/lib/lua/ssl.lua \
	usr/lib/lua/ssl.so \
	usr/lib/lua/ssl \
	usr/lib/lua/ltn12.lua \
	usr/lib/lua/ubus.so \
	usr/lib/lua/uci.so \
	usr/lib/lua/mosquitto.so \
	usr/lib/lua/lfs.so; do
	copy_optional "$path"
done

for pkg in \
	bdinfo cloudd mqttagent mosquitto-nossl libmosquitto-nossl \
	report_proactively libubox20210516 libubus20210630 libjson-c5 \
	libopenssl1.1 nradio-utils libbdinfo lua-cjson lua-md5 libubus-lua \
	mosquitto-lua; do
	copy_opkg_info "$pkg"
	append_status "$pkg"
done

# Keep cloud login/status/signal path, remove remote shell/flash/reset/file writers.
rm -f \
	"$root/usr/lib/cloudd/command" \
	"$root/usr/lib/cloudd/cmd" \
	"$root/usr/lib/cloudd/file" \
	"$root/usr/lib/cloudd/firmware" \
	"$root/usr/lib/cloudd/upgrade" \
	"$root/usr/lib/cloudd/reset" \
	"$root/usr/lib/cloudd/reboot" \
	"$root/usr/lib/cloudd/appstore" \
	"$root/usr/lib/cloudd/develop" \
	"$root/usr/lib/cloudd/bdinfo" \
	"$root/usr/lib/cloudd/pass" \
	"$root/usr/lib/cloudd/password" \
	"$root/usr/lib/cloudd/config" \
	"$root/usr/sbin/kpshd" \
	"$root/etc/init.d/kpsh" \
	"$root/etc/uci-defaults/02-kpsh"
rm -f "$root"/etc/rc.d/*kpsh* \
	"$root"/etc/rc.d/*cpesel* \
	"$root"/etc/rc.d/*simcom_http* \
	"$root"/etc/rc.d/*factory_siminfo* \
	"$root"/etc/rc.d/*command* \
	"$root"/etc/rc.d/*reset* \
	"$root"/etc/rc.d/*upgrade*

mkdir -p "$root/etc/rc.d" "$root/etc/uci-defaults"
ln -sf ../init.d/cloudd "$root/etc/rc.d/S29cloudd"
ln -sf ../init.d/mosquitto "$root/etc/rc.d/S30mosquitto"
ln -sf ../init.d/mqttagent "$root/etc/rc.d/S32mqttagent"
ln -sf ../init.d/qmodem_init "$root/etc/rc.d/S80qmodem_init"
ln -sf ../init.d/qmodem_reboot "$root/etc/rc.d/S95qmodem_reboot"
ln -sf ../init.d/qmodem_network "$root/etc/rc.d/S99qmodem_network"
ln -sf ../init.d/report_proactively "$root/etc/rc.d/S90report_proactively"

cat > "$root/etc/uci-defaults/29-c2000max-cloud" <<'EOF'
#!/bin/sh

valid_mac() {
	printf '%s' "$1" | grep -iqE '^[0-9a-f]{2}(:[0-9a-f]{2}){5}$'
}

pick_mac() {
	local mac
	mac="$(uci -q get oem.board.id 2>/dev/null || true)"
	valid_mac "$mac" && { printf '%s\n' "$mac"; return; }
	mac="$(bdinfo -m 2>/dev/null | head -n 1 || true)"
	valid_mac "$mac" && { printf '%s\n' "$mac"; return; }
	mac="$(bdinfo -g fac_mac 2>/dev/null | head -n 1 || true)"
	valid_mac "$mac" && { printf '%s\n' "$mac"; return; }
	mac="$(cat /sys/class/net/eth1/address 2>/dev/null || true)"
	valid_mac "$mac" && { printf '%s\n' "$mac"; return; }
	mac="$(cat /sys/class/net/br-lan/address 2>/dev/null || true)"
	valid_mac "$mac" && { printf '%s\n' "$mac"; return; }
	printf '%s\n' '00:66:88:00:00:00'
}

mac="$(pick_mac)"
mac_id="$(printf '%s' "$mac" | tr -d ':')"

uci -q batch <<-UCI
	set oem.board.id='$mac'
	set oem.board.name='WT9303'
	set oem.board.pname='C2000-MAX'
	set oem.board.ptype='rt'
	set oem.board.vendor='nradio'
	set oem.feature.cpe='1'
	set cloudd.config.cellular_cpe='2_1'
	set mqttagent.config.sub_mosca='0'
	set report_proactively.config.interval='2'
	set mosquitto.owrt='owrt'
	set mosquitto.owrt.use_uci='1'
	set mosquitto.mosquitto='mosquitto'
	set mosquitto.mosquitto.log_dest='syslog'
	set mosquitto.mosquitto.log_types='warning'
	set mosquitto.mosquitto.allow_anonymous='1'
	set mosquitto.listener='listener'
	set mosquitto.listener.port='1883'
	set mosquitto.listener.no_remote_access='1'
	delete mosquitto.bridge
	set mosquitto.bridge='bridge'
	set mosquitto.bridge.connection='$mac_id'
	set mosquitto.bridge.keepalive_interval='10'
	set mosquitto.bridge.remote_clientid='$mac_id'
	set mosquitto.bridge.remote_username='router'
	set mosquitto.bridge.address='$mac_id.dev.nradio.com.cn'
	set mosquitto.bridge.notifications='1'
	set mosquitto.bridge.notifications_local_only='1'
	set mosquitto.bridge.restart_timeout='5'
	set mosquitto.bridge.try_private='0'
	delete firewall.ate
UCI
uci -q delete mosquitto.bridge.topic
uci -q add_list mosquitto.bridge.topic="sysinfo/# out 1 \"\" $mac_id/"
uci -q add_list mosquitto.bridge.topic="exec/# both 1 rpc/ $mac_id/"
uci -q add_list mosquitto.bridge.topic="kp/mosca/$mac_id/# out 1 \"\" \"\""
uci -q add_list mosquitto.bridge.topic="kp/$mac_id/+/# in 1 \"\" \"\""
uci -q add_list mosquitto.bridge.topic="mosca/# in 1 kp/$mac_id/ kp/FFFFFFFFFFFF/"
uci -q commit oem
uci -q commit cloudd
uci -q commit mqttagent
uci -q commit report_proactively
uci -q commit mosquitto
uci -q commit firewall
exit 0
EOF
chmod 755 "$root/etc/uci-defaults/29-c2000max-cloud"

cat > "$root/usr/bin/at" <<'EOF'
#!/bin/bash
dev="$1"
shift || true
cmd="$*"
[ -n "$dev" ] && [ -n "$cmd" ] || exit 1
[ -e "$dev" ] || exit 1

oldstty="$(stty -F "$dev" -g 2>/dev/null || true)"
stty -F "$dev" 115200 raw -echo -echoe -echok min 0 time 10 2>/dev/null || true

exec 3<>"$dev" || exit 1
printf '%s\r' "$cmd" >&3

end=$(( $(date +%s) + 3 ))
while [ "$(date +%s)" -le "$end" ]; do
	IFS= read -r -t 1 line <&3 || true
	[ -n "$line" ] || continue
	printf '%s\n' "$line"
	case "$line" in
		*OK*|*ERROR*) break ;;
	esac
done

exec 3>&-
[ -n "$oldstty" ] && stty -F "$dev" "$oldstty" 2>/dev/null || true
exit 0
EOF

cat > "$root/usr/bin/tom_modem" <<'EOF'
#!/bin/bash
dev=
cmd=
op=a
while [ $# -gt 0 ]; do
	case "$1" in
		-u) shift ;;
		-d) dev="$2"; shift 2 ;;
		-o) op="$2"; shift 2 ;;
		-c) cmd="$2"; shift 2 ;;
		-p) cmd="$2"; shift 2 ;;
		*) shift ;;
	esac
done

case "$op" in
	a)
		[ -n "$dev" ] && [ -n "$cmd" ] || exit 1
		exec /usr/bin/at "$dev" "$cmd"
		;;
	r)
		printf '{}\n'
		;;
	s|d)
		exit 1
		;;
	*)
		exit 1
		;;
esac
EOF

cat > "$root/usr/bin/sms_tool_q" <<'EOF'
#!/bin/bash
dev=
timeout=
while [ $# -gt 0 ]; do
	case "$1" in
		-d) dev="$2"; shift 2 ;;
		-t) timeout="$2"; shift 2 ;;
		at) shift; exec /usr/bin/at "$dev" "$*" ;;
		recv) printf '{}\n'; exit 0 ;;
		send|delete) exit 1 ;;
		*) shift ;;
	esac
done
exit 1
EOF

cat > "$root/usr/bin/jq" <<'EOF'
#!/usr/bin/lua
local ok, cjson = pcall(require, "cjson.safe")
if not ok then cjson = require "cjson" end

local raw, slurp = false, false
local vars, files = {}, {}
local filter
local args = {...}
local i = 1
while i <= #args do
	local a = args[i]
	if a == "-r" then
		raw = true
	elseif a == "-c" then
		-- compact is default here
	elseif a == "-s" then
		slurp = true
	elseif a:match("^%-[rcs]+$") then
		if a:find("r", 1, true) then raw = true end
		if a:find("s", 1, true) then slurp = true end
	elseif a == "--arg" then
		vars[args[i + 1] or ""] = args[i + 2] or ""
		i = i + 2
	elseif not filter then
		filter = a
	else
		files[#files + 1] = a
	end
	i = i + 1
end

local function read_all(path)
	local f
	if path then f = io.open(path, "r") else f = io.stdin end
	if not f then return "" end
	local data = f:read("*a") or ""
	if path then f:close() end
	return data
end

local input = files[1] and read_all(files[1]) or read_all(nil)
local data = cjson.decode(input)

local function count_table(t)
	local n = 0
	for _ in pairs(t or {}) do n = n + 1 end
	return n
end

local function emit(value, empty)
	if value == nil then
		if not empty then print("null") end
	elseif type(value) == "table" then
		print(cjson.encode(value))
	elseif type(value) == "boolean" then
		print(value and "true" or "false")
	else
		print(tostring(value))
	end
end

local function path_tokens(path)
	path = (path or ""):gsub("^%.", "")
	local tokens = {}
	local p = 1
	while p <= #path do
		local ch = path:sub(p, p)
		if ch == "." then
			p = p + 1
		elseif ch == '"' then
			local q = path:find('"', p + 1, true)
			tokens[#tokens + 1] = path:sub(p + 1, (q or p + 1) - 1)
			p = (q or p) + 1
		elseif ch == "[" then
			local q = path:find("%]", p)
			local key = path:sub(p + 1, (q or p + 1) - 1)
			if key:sub(1, 1) == "$" then key = vars[key:sub(2)] end
			key = key:gsub('^"', ''):gsub('"$', '')
			tokens[#tokens + 1] = key
			p = (q or p) + 1
		else
			local q = p
			while q <= #path and path:sub(q, q) ~= "." and path:sub(q, q) ~= "[" do q = q + 1 end
			tokens[#tokens + 1] = path:sub(p, q - 1)
			p = q
		end
	end
	return tokens
end

local function eval_path(obj, path)
	local cur = obj
	for _, key in ipairs(path_tokens(path)) do
		if type(cur) ~= "table" then return nil end
		cur = cur[key]
	end
	return cur
end

filter = filter or "."

if slurp and filter == "add" then
	local merged = {}
	for chunk in input:gmatch("%b{}") do
		local obj = cjson.decode(chunk)
		if type(obj) == "table" then
			for k, v in pairs(obj) do merged[k] = v end
		end
	end
	emit(merged)
	return
end

if filter:match("^index%(%$(%w+)%)$") then
	local needle = vars[filter:match("^index%(%$(%w+)%)$")]
	if type(data) == "table" then
		for idx, value in ipairs(data) do
			if tostring(value) == tostring(needle) then emit(idx - 1); return end
		end
	end
	emit(nil)
	return
end

if filter == "length" then
	emit(type(data) == "table" and count_table(data) or #(tostring(data or "")))
	return
end

local contains = filter:match('^contains%(%["([^"]+)"%]%)$')
if contains then
	local found = false
	if type(data) == "table" then
		for _, value in ipairs(data) do
			if tostring(value) == contains then found = true end
		end
	end
	emit(found)
	return
end

local select_key = filter:match('select%(%.[Kk]ey=="([^"]+)"%)')
if select_key then
	local field = filter:match('|%s*%.([%w_%-]+)%s*$') or "value"
	local list = eval_path(data, filter:match('^(.-)%[%]')) or {}
	for _, item in ipairs(list) do
		if item.key == select_key then emit(item[field], true); return end
	end
	return
end

if filter:find("to_entries") or filter:find("@sh") then
	return
end

local empty = false
local path = filter
local before_empty = filter:match("^(.-)%s*//%s*empty%s*$")
if before_empty then path = before_empty; empty = true end

local array_path = path:match("^(.-)%[%]%s*$")
if array_path then
	local arr = eval_path(data, array_path)
	if type(arr) == "table" then
		for _, value in ipairs(arr) do emit(value, empty) end
	end
	return
end

if path == "." then
	emit(data, empty)
else
	emit(eval_path(data, path), empty)
end
EOF
chmod 755 "$root/usr/bin/at" "$root/usr/bin/tom_modem" "$root/usr/bin/sms_tool_q" "$root/usr/bin/jq"

for file in \
	"$root/usr/libexec/rpcd/qmodem" \
	"$root/usr/libexec/rpcd/modem_ctrl" \
	"$root/usr/share/qmodem/modem_ctrl.sh" \
	"$root/usr/share/qmodem/modem_dial.sh" \
	"$root/usr/share/qmodem/modem_hook.sh" \
	"$root/usr/share/qmodem/modem_scan.sh" \
	"$root/usr/share/qmodem/modem_util.sh" \
	"$root/usr/share/qmodem/generic.sh" \
	"$root/usr/share/qmodem/vendor/"*.sh; do
	[ -f "$file" ] || continue
	sed -i -e '1s|^#!/bin/sh|#!/bin/bash|' -e '1s|^#/bin/sh$|#!/bin/bash|' "$file"
done
[ ! -f "$root/etc/init.d/qmodem_init" ] || sed -i 's#"ash" "/usr/share/qmodem/modem_scan.sh"#"bash" "/usr/share/qmodem/modem_scan.sh"#g' "$root/etc/init.d/qmodem_init"

cat >> "$root/usr/lib/lua/luci/nradio.lua" <<'EOF'

-- MYOS C2000MAX cloud compatibility overrides.
do
	local util = require "luci.util"
	local uci = require "luci.model.uci".cursor()
	local nixio = require "nixio"

	local function trim(value)
		return (tostring(value or ""):gsub("^%s+", ""):gsub("%s+$", ""))
	end

	local function qmodem_section()
		local section
		uci:foreach("qmodem", "modem-device", function(s)
			if not section and (s.state == "enabled" or not s.state) then
				section = s[".name"]
				return false
			end
		end)
		return section or (uci:get("qmodem", "2_1") and "2_1") or "2_1"
	end

	local function qmodem_call(method, data)
		data = data or {}
		data.config_section = data.config_section or qmodem_section()
		return util.ubus("qmodem", method, data)
	end

	function fork_exec(command)
		local pid = nixio.fork()
		if pid and pid > 0 then
			return pid
		elseif pid == 0 then
			local null = nixio.open("/dev/null", "w+")
			if null then
				nixio.dup(null, nixio.stderr)
				nixio.dup(null, nixio.stdout)
				nixio.dup(null, nixio.stdin)
				if null:fileno() > 2 then null:close() end
			end
			if type(command) == "function" then
				command()
			elseif type(command) == "string" then
				os.execute(command)
			end
			os.exit(0)
		end
	end

	function support_mesh() return false end
	function support_wifiauth() return false end
	function get_platform() return "huawei" end
	function get_cellular_prefix() return "2_1", "2_1" end
	function get_combo_info() return { list = {} } end
	function get_speedlimit_info() return { list = {} } end
	function ubus_send() return true end

	function list_wired_clients()
		local clients = {}
		local ok, stat = pcall(require, "luci.tools.status")
		local leases = ok and stat.dhcp_leases and stat.dhcp_leases() or {}
		for _, lease in ipairs(leases or {}) do
			clients[#clients + 1] = {
				mac = (lease.macaddr or ""):upper(),
				macaddr = (lease.macaddr or ""):upper(),
				ip = lease.ipaddr or "",
				ipaddr = lease.ipaddr or "",
				hostname = lease.hostname or ""
			}
		end
		return { count = #clients, client = clients }
	end

	function get_local_device()
		local section = qmodem_section()
		local network = uci:get("qmodem", section, "network") or section
		local ifstatus = util.ubus("network.interface." .. network, "status") or {}
		return {
			{
				name = network,
				ifname = ifstatus.device or network,
				proto = "qmodem",
				up = ifstatus.up and true or false,
				modem = section
			}
		}
	end

	function get_terminal_list()
		local wired = list_wired_clients()
		return wired.client or {}, 0
	end

	function get_client_switch() return 1 end
	function set_client_switch() return true end
	function set_client_name() return true end
	function set_password_info() return 1 end
	function deal_service() return { code = -1, enabled = 0 } end

	function switch_simcard(simcard)
		local slot = tonumber(simcard)
		if slot and slot > 0 then slot = slot - 1 end
		if slot ~= 0 and slot ~= 1 then return false end
		local res = qmodem_call("set_sim_slot", { slot = tostring(slot) })
		return res ~= nil
	end

	function get_net_status()
		local section = qmodem_section()
		local network = uci:get("qmodem", section, "network") or section
		local ifstatus = util.ubus("network.interface." .. network, "status") or {}
		return {{ name = network, is_up = ifstatus.up and true or false }}
	end
end
EOF

cat >> "$root/usr/lib/lua/cloudd/api.lua" <<'EOF'

-- MYOS C2000MAX qmodem/cloud compatibility overrides.
local function myos_str(value)
	return (tostring(value or ""):gsub("^%s+", ""):gsub("%s+$", ""))
end

local function myos_pick(map, ...)
	for i = 1, select("#", ...) do
		local key = select(i, ...)
		local value = map[key]
		if value ~= nil and myos_str(value) ~= "" and myos_str(value) ~= "N/A" then
			return myos_str(value)
		end
	end
	return ""
end

local function myos_array_map(data)
	local map = {}
	for _, item in ipairs((data and data.modem_info) or {}) do
		local key = item.key or item.name or item.title
		if key then
			map[key] = item.value or item.val or item.result or ""
		end
	end
	for k, v in pairs(data or {}) do
		if type(v) ~= "table" then map[k] = v end
	end
	return map
end

local function myos_find_qmodem()
	local section, network
	uci:foreach("qmodem", "modem-device", function(s)
		if not section and (s.state == "enabled" or not s.state) then
			section = s[".name"]
			network = s.network or s[".name"]
			return false
		end
	end)
	if not section and uci:get("qmodem", "2_1") then
		section = "2_1"
		network = uci:get("qmodem", section, "network") or "2_1"
	end
	section = section or "2_1"
	network = network or section
	return section, network
end

local function myos_qmodem(method, section)
	section = section or myos_find_qmodem()
	return util.ubus("qmodem", method, { config_section = section }) or {}
end

local function myos_ifstatus(name)
	return util.ubus("network.interface." .. name, "status") or {}
end

local function myos_first_ip(status, key)
	for _, item in ipairs(status[key] or {}) do
		if item.address then return item.address end
	end
	return ""
end

local function myos_qmodem_summary()
	local section, network = myos_find_qmodem()
	local base = myos_array_map(myos_qmodem("base_info", section))
	local sim = myos_array_map(myos_qmodem("sim_info", section))
	local net = myos_array_map(myos_qmodem("network_info", section))
	local cell = myos_array_map(myos_qmodem("cell_info", section))
	local conn = myos_array_map(myos_qmodem("get_connect_status", section))
	local slot = myos_array_map(myos_qmodem("get_sim_slot", section))
	local if4 = myos_ifstatus(network)
	local if6 = myos_ifstatus(network .. "v6")
	local up = if4.up or if6.up or conn.connection_status == "connected" or conn.connection_status == "1"
	local sim_slot = myos_pick(slot, "sim_slot", "slot", "result")
	local slot_num = tonumber(sim_slot) or 0

	return {
		section = section,
		network = network,
		ifname = if4.device or if6.device or uci:get("qmodem", section, "network") or network,
		up = up and true or false,
		uptime = tonumber(if4.uptime or if6.uptime or 0) or 0,
		ipv4 = myos_first_ip(if4, "ipv4-address"),
		ipv6 = myos_first_ip(if6, "ipv6-address"),
		imei = myos_pick(base, "IMEI", "imei"),
		iccid = myos_pick(sim, "ICCID", "iccid", "SimICCID"),
		simid = myos_pick(sim, "IMSI", "imsi", "SIMID", "simid"),
		operator = myos_pick(net, "Operator", "operator", "ISP", "isp", "COPS") ~= "" and myos_pick(net, "Operator", "operator", "ISP", "isp", "COPS") or myos_pick(cell, "Operator", "operator", "ISP", "isp", "MCC", "MMC", "MNC"),
		mode = myos_pick(net, "Mode", "mode", "MODE", "RAT", "rat", "Network", "network") ~= "" and myos_pick(net, "Mode", "mode", "MODE", "RAT", "rat", "Network", "network") or myos_pick(cell, "Mode", "mode", "MODE", "network_mode", "RAT", "rat"),
		rsrp = myos_pick(cell, "RSRP", "rsrp"),
		rsrq = myos_pick(cell, "RSRQ", "rsrq"),
		rssi = myos_pick(cell, "RSSI", "rssi"),
		sinr = myos_pick(cell, "SINR", "sinr", "SNR", "snr"),
		pci = myos_pick(cell, "PCI", "pci"),
		band = myos_pick(cell, "NR_BAND", "NR-SA_BAND", "NR-NSA_BAND", "LTE_BAND", "Band", "BAND", "band", "NR Band", "LTE Band"),
		sim_slot = tostring(slot_num),
		app_slot = tostring(slot_num + 1)
	}
end

local function myos_cpe_item()
	local m = myos_qmodem_summary()
	return {
		name = m.network,
		cpeno = 1,
		ifname = m.ifname,
		status = m.up and "register" or "offline",
		mode = m.mode,
		isp = m.operator,
		operator = m.operator,
		iccid = m.iccid,
		simid = m.simid,
		imei = m.imei,
		sim = m.app_slot,
		slot = m.app_slot,
		rsrp = m.rsrp,
		rsrq = m.rsrq,
		rssi = m.rssi,
		sinr = m.sinr,
		pci = m.pci,
		band = m.band,
		ip = m.ipv4,
		ipaddr = m.ipv4,
		ipv6 = m.ipv6
	}
end

function cloudd_get_cpesel()
	local m = myos_qmodem_summary()
	return {{ mode = "1", cur = m.app_slot, iccid = m.iccid, rsrp = m.rsrp, rsrq = m.rsrq, rssi = m.rssi, sinr = m.sinr }}
end

function cloudd_get_cpe()
	return { list = { myos_cpe_item() } }
end

function cloudd_get_int()
	local m = myos_qmodem_summary()
	local value = { mode = m.mode, time = m.uptime }
	if m.ipv4 ~= "" then value.ip = m.ipv4 end
	if m.ipv6 ~= "" then value.ipv6 = m.ipv6 end
	return value
end

function cloudd_get_traffic()
	return { list = {} }
end

function cloudd_get_limitstatus()
	return { list = {} }
end

function cloudd_get_wanstatus()
	local item = myos_cpe_item()
	return { list = {{ name = item.name, status = item.status, mode = item.mode, ipaddr = item.ipaddr, ipv6 = item.ipv6 }} }
end

function cloudd_get_linkstatus()
	local item = myos_cpe_item()
	return { list = {{ name = item.name, link = item.status == "register" and 1 or 0, mode = item.mode, rsrp = item.rsrp, rsrq = item.rsrq, rssi = item.rssi, sinr = item.sinr }} }
end

function cloudd_get_status(brief)
	local value = {}
	local nr = require "luci.nradio"
	local s_info = util.ubus("system", "info") or {}
	local m = myos_qmodem_summary()
	local board_name = uci:get("oem", "board", "name") or "WT9303"
	local board_pname = uci:get("oem", "board", "pname") or "C2000-MAX"

	value.sversion = cloudd_get_local_sversion()
	value.board = board_name
	value.name = board_pname
	value.ptype = uci:get("oem", "board", "ptype") or "rt"
	value.vendor = uci:get("oem", "board", "vendor") or "nradio"
	value.uptime = s_info.uptime or 0
	value.iccid = {{ m.iccid or "" }}
	value.simid = {{ m.simid or "" }}
	value.cpesel = cloudd_get_cpesel()
	value.cpe = myos_cpe_item()
	value.wired_client = nr.list_wired_clients and nr.list_wired_clients() or { count = 0, client = {} }

	if brief == 1 then
		return value
	end

	value.id = cloudd_get_self_id()
	value.oid = cloudd_get_owner_id()
	value.ifinfo = nr.get_local_device and nr.get_local_device() or {}
	value.ipaddr = m.ipv4 ~= "" and m.ipv4 or (uci:get("network", "lan", "ipaddr") or "")
	if m.ipv6 ~= "" then value.ipv6 = m.ipv6 end

	local ok, radio, radiocnt = pcall(cloudd_generate_local_radio)
	if ok then
		value.radio = radio
		value.radiocnt = radiocnt
	else
		value.radio = {}
		value.radiocnt = {}
	end

	local cl_raw = cloudd_ubus_cloudd_client() or { count = 0, client = {} }
	local cl = {}
	for i = 1, tonumber(cl_raw.count or 0) do
		local client = cl_raw.client and cl_raw.client[i]
		if client then
			if client.radio and type(client.radio) == "string" then
				pcall(function() client.radio = cjson.decode(client.radio) or {} end)
			end
			table.insert(cl, client)
		end
	end
	value.client = cl
	return value
end

function cloudd_apply_command(d_id, p_data, topic, proto)
	p_data = p_data or {}
	p_data.result = "# command disabled on MYOS #"
	p_data["return"] = "1"
	cloudd.cloudd_send(d_id, topic or "command", p_data, proto, 1)
end

function cloudd_apply_cmd(d_id, p_data, topic, proto)
	p_data = p_data or {}
	p_data.result = "# cmd disabled on MYOS #"
	p_data.code = 1
	cloudd.cloudd_send(d_id, topic or "cmd", p_data, proto, 1)
end

function cloudd_do_reboot(d_id, proto)
	cloudd.cloudd_send(d_id, "reboot", { code = 1, result = "disabled on MYOS" }, proto, 1)
end

function cloudd_do_reset(d_id, proto)
	cloudd.cloudd_send(d_id, "reset", { code = 1, result = "disabled on MYOS" }, proto, 1)
end

function cloudd_do_upgrade(d_id, p_data, proto)
	cloudd.cloudd_send(d_id, "upgrade", { code = 1, result = "disabled on MYOS" }, proto, 1)
end

function cloudd_handle_upgrade(d_id, p_data, proto)
	cloudd.cloudd_send(d_id, "firmware", { code = 1, result = "disabled on MYOS" }, proto, 1)
end

function cloudd_remote_download()
	return false
end

function cloudd_remote_upgrade()
	return false
end
EOF

chmod 755 "$root/etc/init.d/cloudd" "$root/etc/init.d/mosquitto" \
	"$root/etc/init.d/mqttagent" "$root/etc/init.d/report_proactively" \
	"$root/etc/init.d/pcie_mhi" "$root/etc/init.d/qmodem_init" \
	"$root/etc/init.d/qmodem_network" "$root/etc/init.d/qmodem_reboot" \
	"$root/usr/bin/cloudd" "$root/usr/bin/mqttagent" \
	"$root/usr/sbin/mosquitto" "$root/usr/sbin/report_proactively" \
	"$root/usr/bin/bdinfo" "$root/usr/libexec/rpcd/qmodem" \
	"$root/usr/libexec/rpcd/modem_ctrl"
find "$root/usr/share/qmodem" -type f -name '*.sh' -exec chmod 755 {} +

exit 0
