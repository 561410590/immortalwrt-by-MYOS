#!/bin/ash
. /lib/functions.sh
. /lib/network/switch.sh
. /usr/share/libubox/jshn.sh

dWait=12
dMode=0
dCtrlCode=
gWait=$dWait
gMode=$dMode
gHasWan=
gLanPorts=
gLedStatus=
gLedError=
gLedCloud=
gLedInt=
gLedMeshAct=
gLedMeshSt=
gLedMeshIf=
gLedMode=
gLedWifi=
gLedSim=
gLedSig1=
gLedSig2=
gLedSig3=
gLedSig=
gLedCmode=
gLedCmode5=
gLedCmode4=
gCtrlCode=
gCpeStat=
gNetStat=
gMeshRole=
gMeshStat=
gMeshOldStat=
gMeshRssi="NA"
gMeshAgents="NA"
gIsMeshEnabled=
gWanList=
gLock=""
gForbidMesh=$(uci -q get oem.forbidden.mesh)
gRunning=
gTrigger=
gWiredOnly="6"
gOnlineType=""
gNow=0
gIfaceData=
gWanData=
gCellName=
gCellRsrp=
gCellRadio=
gCellStat=
gCellSim=
gCellAt=0
gSigLevel=
gSigNext=
gSigAt=0
gIntAt=0
gIntState=0
gIntDev=
wan6=$(uci -q get network.globals.default_wan6)
[ -z "$wan6" ] && wan6="wan6"

trap check_cur_sim USR1

usage() {
	cat <<-EOF
		Usage: $0 OPTION...
		Led control daemon.

		  -w		wait period, use '$dWait' seconds as default
		  -m		led mode, 0: normal, 1: disable, use '$dMode' as default
	EOF
}

trap trigger_led USR1

async_sleep() {
	sleep "$1" &
	wait $!
}

init_dev() {
	local _name=$1

	find /sys/class/leds -maxdepth 1 -mindepth 1 \
		|grep -sE "^.*/[a-z]+:[0-9a-z]+:${_name}[0-9]*$"
}

disable_all_leds() {
	local _leds=$(find /sys/class/leds -maxdepth 1 -mindepth 1)

	for _led in $_leds; do
		onoff_led $_led 0
	done
}

init_total_dev() {
	local _name=$1

	find /sys/class/leds -maxdepth 1 -mindepth 1 \
		|grep -sE "^.*/[a-z]+:[0-9a-z]+:${_name}$"
}

init_mesh_dev() {
	local _role=$gMeshRole
	local _dev=$(init_dev "mesh")
	local _wlan0_ifname=$(uci -q get wireless.wlan0.ifname)
	local _wlan1_ifname=$(uci -q get wireless.wlan1.ifname)

	if [ "$_role" = "1" ] && [ "$gForbidMesh" != "1" ]; then
		gLedMeshIf="apcli${_wlan0_ifname#ra} apcli${_wlan1_ifname#ra}"
	else
		gLedMeshIf="$_wlan0_ifname $_wlan1_ifname"
	fi

	if [ -z "$_dev" ]; then
		if [ "$_role" = "0" ]; then
			gLedMeshAct="$gLedStatus"
			gLedMeshSt="$gLedStatus"
		else
			gLedMeshAct="$gLedError"
			gLedMeshSt="$gLedStatus"
		fi
	else
		gLedMeshAct="$_dev"
		gLedMeshSt="$_dev"
	fi
}

check_lan() {
	local _ports=$1

	_ports=$(echo "$_ports" | sed "s/ //g")

	nrswitch -g | grep -E "^[$_ports]" | grep -q "up"
}

check_mesh_topology_info() {
	local _type=
	local _rssi=

	mapd_cli /tmp/mapd_ctrl dump_topology_v1 >/dev/null 2>&1
	_type=$(jsonfilter -e '$["topology information"][0]["BH Info"][0]["Backhaul Medium Type"]' < /tmp/dump.txt)
	_rssi=$(jsonfilter -e '$["topology information"][0]["BH Info"][0]["RSSI"]' < /tmp/dump.txt)

	echo "$_type,$_rssi"
}

update_mesh_status() {
	local _role=$gMeshRole
	local _enabled=
	local _config=
	local _iface=
	local _status="0"
	local _tmp=
	local _type=

	_enabled=$(uci -q get mesh.config.enabled)
	_config=$(uci -q get mesh.config.config)

	gMeshOldStat=$gMeshStat
	if [ "$_enabled" != "1" ]; then
		gMeshStat=0

		if [ "$gMeshStat" != "$gMeshOldStat" ]; then
			logger -t "ledctrl" "mesh status: $gMeshOldStat -> $gMeshStat"
		fi

		return
	fi

	if [ "$_role" = "1" ] && [ "$_config" = "1" ]; then
		gMeshStat=5
		_tmp=$(check_mesh_topology_info)
		_type=$(echo "$_tmp"|cut -d, -f1)

		if [ "$_type" = "Ethernet" ]; then
			gMeshStat=3
		else
			for _iface in $gLedMeshIf; do
				if iwconfig "$_iface"| \
						grep "Access Point"| \
						grep -iqE "([0-9a-f]{2}:){5}[0-9a-f]{2}"; then
					gMeshStat=3
					break
				fi
			done
		fi

		if [ "$gMeshStat" != "$gMeshOldStat" ]; then
			logger -t "ledctrl" "mesh status: $gMeshOldStat -> $gMeshStat"
		fi

		return
	fi

	gMeshStat=0
	for _iface in $gLedMeshIf; do
		if [ "$gMeshStat" != "0" ]; then
			break
		fi
		_tmp=$(iwpriv "$_iface" stat|grep WscStatus|awk -F"= " '{print $2}')
		if [ "$_tmp" -le 1 ]; then
			gMeshStat=0
		elif [ "$_tmp" -eq 3 ] || [ "$_tmp" -eq 35 ] || [ "$_tmp" -eq 39 ]; then
			gMeshStat=1
		elif [ "$_tmp" -gt 3 ] && [ "$_tmp" -lt 30 ]; then
			gMeshStat=2
		elif [ "$_tmp" -eq 34 ]; then
			gMeshStat=3
		elif [ "$_tmp" -eq 2 ]; then
			gMeshStat=4
		fi
	done

	if [ "$gMeshStat" != "$gMeshOldStat" ]; then
		logger -t "ledctrl" "mesh status: $gMeshOldStat -> $gMeshStat"
	fi
}

update_wps_status() {
	local _iface=
	local _tmp=

	gMeshOldStat=$gMeshStat
	gMeshStat=0
	for _iface in $gLedMeshIf; do
		if [ "$gMeshStat" != "0" ]; then
			break
		fi
		_tmp=$(iwpriv "$_iface" stat|grep WscStatus|awk -F"= " '{print $2}')
		if [ "$_tmp" -eq 34 ]; then
			gMeshStat=2
		elif [ "$_tmp" -eq 3 ]; then
			gMeshStat=1
		fi
	done

	if [ "$gMeshStat" != "$gMeshOldStat" ]; then
		logger -t "ledctrl" "wps status: $gMeshOldStat -> $gMeshStat"
	fi
}

check_agents() {
	local _agents=
	local _local_id=

	if [ "$gMeshRole" = "1" ]; then
		return
	fi

	if [ "$gLedMeshSt" != "$gLedStatus" ]; then
		_local_id=$(uci -q get oem.board.id)
		_agents=$(ubus call cloudd client | grep '\"id\"' | grep -v `echo $_local_id | tr -d ":"` | wc -l)

		if [ "$_agents" != "0" ]; then
			if [ "$gMeshAgents" = "NA" ] || [ "$gMeshAgents" = "0" ]; then
				onoff_led "$gLedMeshSt" 1
			fi
		else
			if [ "$gMeshAgents" = "NA" ] || [ "$gMeshAgents" != "0" ]; then
				onoff_led "$gLedMeshSt" 0
			fi
		fi

		gMeshAgents=$_agents
	elif [ "$gMeshStat" != "$gMeshOldStat" ]; then
		onoff_led "$gLedMeshSt" 1
	fi
}

check_easy_mesh_sig() {
	local _role=$gMeshRole
	local _tmp=
	local _type=
	local _rssi=

	if [ "$_role" = "0" ]; then
		check_agents
	else
		_tmp=$(check_mesh_topology_info)
		_type=$(echo "$_tmp"|cut -d, -f1)
		_rssi=$(echo "$_tmp"|cut -d, -f2)
		check_mesh_int
		if [ "$_type" = "Ethernet" ]; then
			onoff_led "$gLedMeshSt" 1
		else
			if [ "$_rssi" = "" ]; then
				_rssi=$gMeshRssi
			fi
			if [ "$_rssi" = "NA" ]; then
				_rssi=-100
			elif [ "$_rssi" -eq -127 ] || [ "$_rssi" -eq -110 ]; then
				_rssi=0
			fi
			if [ "$_rssi" -ge -70 ]; then
				if [ "$gMeshRssi" = "NA" ] || [ "$gMeshRssi" -lt -70 ]; then
					onoff_led "$gLedMeshSt" 1
				fi
			else
				if [ "$gMeshRssi" = "NA" ] || [ "$gMeshRssi" -ge -70 ]; then
					set_led "$gLedMeshSt" "timer" 1000 1000
				fi
			fi
		fi
		gMeshRssi=$_rssi
	fi
}

check_mesh_int() {
	if ubus -S call wanchk get '{"name":"mesh"}'|grep -qsw "up" ; then
		[ -n "$gLedInt" ] && onoff_led "$gLedInt" 1
	else
		[ -n "$gLedInt" ] && onoff_led "$gLedInt" 0
	fi
}

check_easy_mesh(){
	local _agents=

	update_mesh_status

	if [ "$gMeshStat" = "$gMeshOldStat" ] && [ "$gMeshStat" != "3" ]; then
		if [ "$gMeshRole" = "1" ] || [ "$gMeshStat" != "4" -a "$gMeshStat" != "0" ]; then
			return
		fi
	fi

	if [ "$gMeshRole" = "1" ] && [ "$gMeshStat" != "3" ]; then
		[ -n "$gLedInt" ] && onoff_led "$gLedInt" 0
	fi

	if [ "$gMeshStat" = "1" ]; then
		set_led "$gLedMeshAct" "timer" "200" "200"
		gMeshAgents="NA"
		gMeshRssi="NA"
	elif [ "$gMeshStat" = "2" ]; then
		set_led "$gLedMeshAct" "timer" "100" "100"
		gMeshAgents="NA"
		gMeshRssi="NA"
	elif [ "$gMeshStat" = "3" ]; then
		check_easy_mesh_sig
	elif [ "$gMeshStat" = "4" ]; then
		if [ "$gLedMeshAct" = "$gLedStatus" ] || [ "$gLedMeshAct" = "$gLedError" ]; then
			onoff_led "$gLedMeshAct" 1
		else
			if [ "$gMeshRole" = "1" ]; then
				onoff_led "$gLedMeshAct" 0
			else
				check_agents
			fi
		fi
		gMeshRssi="NA"
	elif [ "$gMeshStat" = "0" ]; then
		if [ "$gLedMeshAct" != "$gLedStatus" ] && [ "$gLedMeshAct" != "$gLedError" ]; then
			if [ "$gMeshRole" = "1" ]; then
				onoff_led "$gLedMeshAct" 0
			else
				check_agents
			fi
		fi
		gMeshRssi="NA"
	elif [ "$gMeshStat" = "5" ]; then
		# controller will not run to here
		set_led "$gLedMeshAct" "timer" "100" "100"
		gMeshAgents="NA"
		gMeshRssi="NA"
	fi
}

check_wps() {
	update_wps_status

	if [ "$gMeshStat" != "$gMeshOldStat" ]; then
		if [ "$gMeshStat" -eq 1 ]; then
			set_led "$gLedMeshAct" "timer" "200" "200"
		elif [ "$gMeshStat" -eq 2 ]; then
			onoff_led "$gLedMeshAct" 1
		else
			onoff_led "$gLedMeshAct" 0
		fi
	fi
}

check_wan() {
	local _dev="$gLedInt"
	local _route _iface _ifaces _key _state _family _pass _primary _target _entry
	local _l3= _word _previous=

	[ -n "$_dev" ] && [ "$gLock" == "1" ] && {
		set_led "$_dev" "timer" "200" "200"
		return 0
	}

	# Prefer the normal egress, then the live policy-routed WANs. A stale
	# linkdown main-table route must not hide a working mwan3/native uplink.
	_ifaces=$(jsonfilter -s "$gIfaceData" -e '@.interface[@.up=true].interface')
	for _family in 4 6; do
		case "$_family" in
			4) _target=1.1.1.1;;
			6) _target=2606:4700:4700::1111;;
		esac
		_route=$(ip -"$_family" route get "$_target" 2>/dev/null)
		_primary= _previous=
		for _word in $_route; do
			[ "$_previous" = dev ] && { _primary=$_word; break; }
			_previous=$_word
		done
		for _pass in primary policy; do
		for _iface in $_ifaces; do
			if [ "$_family" = 4 ]; then
				case "$_iface" in *_6|*6) continue;; esac
			else
				case "$_iface" in *_6|*6) ;; *) continue;; esac
			fi
			_entry=$(jsonfilter -s "$gIfaceData" -e '@.interface[@.interface="'"$_iface"'"]')
			_l3=$(jsonfilter -s "$_entry" -e '@.l3_device')
			[ -n "$_l3" ] || continue
			[ "$_pass" = primary ] && [ "$_l3" != "$_primary" ] && continue
			[ "$_pass" = policy ] && [ "$_l3" = "$_primary" ] && continue
			[ "$(cat "/sys/class/net/$_l3/carrier" 2>/dev/null)" = 0 ] && continue
			_key=${_iface%_4}
			_state=$(jsonfilter -s "$gWanData" -e '@["'"$_key"'"]')
			_route=$(ip -"$_family" route get "$_target" oif "$_l3" 2>/dev/null) || continue
			case " $_route " in *" linkdown "*) continue;; esac
			case " $_route " in *" dev $_l3 "*) ;; *) continue;; esac
			if [ "$_state" != up ] &&
			   [ "$(cat "/var/run/mwan3/iface_state/$_iface" 2>/dev/null)" != online ]; then
				# Native IPv6 checks can be stale/down while IPv6 is usable.
				# Only after all IPv4 paths fail, confirm this IPv6 egress.
				[ "$_family" = 6 ] &&
					ping6 -I "$_l3" -c 1 -W 2 "$_target" >/dev/null 2>&1 || continue
			fi
			case "$_key" in cpe*|wan[0-9]*) gOnlineType=cpe;; *) gOnlineType=wan;; esac
			if [ "$gIntState" != 1 ] || [ "$gIntDev" != "$_l3" ]; then
				logger -t ledctrl "Internet LED up via $_l3 ($_key)"
			fi
			gIntAt=$gNow
			gIntState=1
			gIntDev=$_l3
			onoff_led "$_dev" 1
			return 0
		done
	done
	done
	# A single missing/failed diagnostic must not flicker the Internet LED.
	if [ "$gIntState" = 1 ] && [ $((gNow-gIntAt)) -le 36 ]; then
		onoff_led "$_dev" 1
		return 0
	fi
	if [ "$gIntState" = 1 ]; then
		logger -t ledctrl "Internet LED down: no healthy current egress"
	fi
	gIntState=0
	gOnlineType=""
	onoff_led "$_dev" 0
	return 1
}

check_apcli() {
	local _ifname=
	local _meshen=
	local _iface=

	_ifname=$(uci -q get wireless.apcli.ifname)
	_meshen=$(uci -q get mesh.config.enabled)

	if [ -n "$_ifname" ] && \
			iwconfig "$_ifname"| \
			grep "Access Point"| \
			grep -iqE "([0-9a-f]{2}:){5}[0-9a-f]{2}"; then
		return 0
	fi

	if [ "$_meshen" = "1" ] && \
		   [ "$gMeshRole" = "1" ]; then
		for _iface in $gLedMeshIf; do
			if iwconfig "$_iface"| \
					grep "Access Point"| \
					grep -iqE "([0-9a-f]{2}:){5}[0-9a-f]{2}"; then
				return 0
			fi
		done
	fi

	return 1
}

check_mesh() {
	check_apcli
}

check_wifi_action() {
	if [ ! -f /bin/serial_atcmd ]; then
		iwinfo | grep -qsw "ESSID"
	else
		iw dev | grep -qsw "ssid"
	fi
}

check_wifi() {
	local _dev=

	_dev="$gLedWifi"
	[ -z "$_dev" ] && return 0

	if check_wifi_action; then
		onoff_led "$_dev" 1
	else
		onoff_led "$_dev" 0
	fi
}

check_model() {
	local models="model model1"
	local cpe_cnt=$(uci -q get oem.feature.cpe)

	for model in $models;do
		local _dev=$(init_total_dev "$model")
		[ -n "$_dev" ] && {
			local gIndex=${model##*[A-Za-z]}
			local vendor=""
			local net_check=0
			_info=$(ubus call infocd get "{\"name\":\"cpe${gIndex}_dev\"}"|jsonfilter -e '@.*[@.name="'cpe${gIndex}'_dev"]["parameter"]')
			[ -n "$_info" ] && {
				vendor=$(echo "$_info"|jsonfilter -e '$["vendor"]')
			}

			if [ $cpe_cnt -gt 1 ];then
				local net=$(cat /var/run/mwan3/net_state 2>/dev/null || echo "unknown")
				local gNetIndex=${net##*[A-Za-z]}
				if [ "${net%[0-9]*}" = "cpe" -a "$gNetIndex" = "$gIndex" ];then
					net_check=1
				fi
			else
				net_check=1
			fi
			if [ -n "$vendor" -a $net_check = 1 ];then
				onoff_led "$_dev" 1
			else
				onoff_led "$_dev" 0
			fi
		}
	done
}
update_cell_info() {
	local _next=cpe _iface _up _state _data
	# Only an enabled, online ODU takes priority over the built-in modem.
	if [ "$(uci -q get network.cpe1.mode)" = odu ] &&
	   [ "$(uci -q get network.cpe1.disabled)" != 1 ] &&
	   [ "$(uci -q get network.cpe1.auto)" != 0 ] &&
	   [ "$(cat /sys/class/net/port5/carrier 2>/dev/null)" = 1 ]; then
		for _iface in cpe1_4 cpe1_6; do
			_up=$(jsonfilter -s "$gIfaceData" -e '@.interface[@.interface="'"$_iface"'"].up')
			_state=$(jsonfilter -s "$gWanData" -e '@["'"${_iface%_4}"'"]')
			if [ "$_up" = true ] && [ "$_state" = up ]; then
				_next=cpe1
				break
			fi
		done
	fi
	if [ "$gCellName" != "$_next" ]; then
		logger -t ledctrl "cellular LED source: ${gCellName:-none} -> $_next"
		gCellName=$_next
		gSigLevel=
		gSigNext=
		gSigAt=0
		gCellAt=0
		gCpeStat=
	fi
	gCellStat=
	gCellSim=
	if [ "$gCellName" = cpe1 ]; then
		# ODU registration and signal come from its gateway-matched cache,
		# not the empty local AT record for cpe1.
		_data=$(lua -e 'local s=require("c5800_secondary").cached_signal(); if s then print(require("luci.jsonc").stringify(s)) end')
		gCellRsrp=$(jsonfilter -s "$_data" -e '@.rsrp')
		gCellRadio=$(jsonfilter -s "$_data" -e '@.mode')
		if [ "$(jsonfilter -s "$_data" -e '@.netlink')" = 1 ]; then
			gCellStat=register
			gCellSim=ready
		fi
	else
		_data=$(ubus call infocd cpestatus)
		gCellRsrp=$(jsonfilter -s "$_data" -e '@.result[@.status.name="'"$gCellName"'"].status.rsrp')
		gCellRadio=$(jsonfilter -s "$_data" -e '@.result[@.status.name="'"$gCellName"'"].status.mode')
		_data=$(ubus call infocd cpeinfo "{\"name\":\"$gCellName\"}")
		gCellStat=$(jsonfilter -s "$_data" -e '@.STAT')
		gCellSim=$(jsonfilter -s "$_data" -e '@.SIM')
	fi
}

get_sig_info() {
	[ "$1" = "$gCellName" ] && printf '%s\n' "$gCellRsrp"
}

check_cell_sig() {
	local _rsrp="$gCellRsrp" _level=0
	case "$_rsrp" in
		-*) case "${_rsrp#-}" in ''|*[!0-9]*) _rsrp=;; esac;;
		*) _rsrp=;;
	esac
	if [ -z "$_rsrp" ]; then
		gSigNext=
		if [ "$gSigAt" -gt 0 ] && [ $((gNow-gSigAt)) -le 36 ]; then
			return 0
		fi
		onoff_led "$gLedSig" 0
		gSigLevel=
		gSigNext=
		return 1
	fi
	gSigAt=$gNow
	# Confirm a changed level on two consecutive samples instead of keeping
	# a previous level indefinitely inside a dBm hysteresis band.
	_level=1
	[ "$_rsrp" -ge -90 ] && _level=2
	[ "$_rsrp" -ge -80 ] && _level=3
	if [ -n "$gSigLevel" ] && [ "$_level" != "$gSigLevel" ] &&
	   [ "$gSigNext" != "$_level" ]; then
		gSigNext=$_level
		return 0
	fi
	gSigNext=
	gSigLevel=$_level
	if [ -z "$gSigMode" ]; then
		onoff_led "$gLedSig1" 1
		onoff_led "$gLedSig2" $((_level>=2))
		onoff_led "$gLedSig3" $((_level>=3))
	else
		onoff_led "$gLedSig1" $((_level==3))
		onoff_led "$gLedSig2" $((_level==2))
		onoff_led "$gLedSig3" $((_level==1))
	fi
}

check_cell_stat() {
	local _regstat=
	local _cmode_dev=
	local _tmp=

	local STAT="$1"
	local MODE="$2"
	local SIM="$3"
	local name="$4"
	[ -z "$gLedSig" ] && [ -z "$gLedCmode" ] && return 0

	for _tmp in $gLedCmode; do
		if [ "$gLock" == "1" ]; then
			set_led "$_tmp" "timer" "200" "200"
		fi
	done

	# Before the first online sample, show the initial network search.
	# Afterwards only an explicit unregister report starts the flashing.
	if [ "$gIntAt" -eq 0 ] && [ "$gIntState" != 1 ]; then
		STAT=unregister
	fi

	if [ "$STAT" != register ] && [ "$gCellAt" -gt 0 ] && [ $((gNow-gCellAt)) -le 36 ]; then
		return 0
	fi
	if [ "$STAT" = "register" ]; then
		gCellAt=$gNow
		[ -n "$MODE" ] || MODE=$gCellRadio
		if echo "$MODE" |grep -q "NR"; then
			_cmode_dev="$gLedCmode5"
		elif [ "$MODE" = "LTE" ]; then
			_cmode_dev="$gLedCmode4"
		fi

		if [ "$gLock" != "1" ] ;then
			for _tmp in $gLedCmode; do
				if [ "$_tmp" = "$_cmode_dev" ]; then
					onoff_led "$_cmode_dev" 1
				else
					onoff_led "$_tmp" 0
				fi
			done
		fi

		gCpeStat="register"
	elif [ "$STAT" = "unregister" ]; then
		if [ "$gCpeStat" != "unregister" ]; then
			gSigLevel=
			gSigNext=
			gSigAt=0
			flash_leds "$gLedSig"
			logclient -i custom -m "[module] register error"
			[ "$gLock" != "1" ] && onoff_led "$gLedCmode" 0
		else
			_net_prefer=$(uci -q get network.globals.net_prefer)
			if [ "$_net_prefer" == "$gWiredOnly" ];then 
				onoff_led "$gLedSig" 0
			else
				if [ "$gNetStat" == "$gWiredOnly" ];then
					onoff_led "$gLedSig" 1
				else
					flash_check "$gLedSig"
				fi
			fi
			gNetStat=$_net_prefer
		fi
		gCpeStat="unregister"
	elif [ "$SIM" = "error" ]; then
		onoff_led "$gLedSig" 0
		[ "$gLock" != "1" ] && onoff_led "$gLedCmode" 0
		gCpeStat="not ready"
		logclient -i custom -m "[module] sim recognise error"
	else
		# Missing telemetry is unknown, not a report of network search.
		onoff_led "$gLedSig" 0
		[ "$gLock" != "1" ] && onoff_led "$gLedCmode" 0
		gSigLevel=
		gSigNext=
		gCpeStat="error"
	fi

	if [ "$gCpeStat" = "register" ] && [ -n "$gLedSig" ]; then
		[ -n "$name" ] && check_cell_sig "$name"
	fi
}

multi_gpio_led_control(){
	local _gval=
	local _gpio_path="/sys/class/gpio/cpe-led"
	local _mgl=$1
	local _snum=$2
	local _glen
	_gval="$(echo "$_mgl"|cut -d, -f "$_snum")"
	_glen=${#_gval}
	for _i in $(seq -w "$_glen")
	do
		local _idx=$((_glen-_i))
		local _val=${_gval:$_idx:1}
		[ -f "${_gpio_path}${_i}/value" ] && echo "$_val" > "${_gpio_path}${_i}/value"
	done
}

check_cur_sim() {
	local _snum=
	local _dev=
	local _devs=
	local _tmp=
	local _mgl=

	_snum=$(uci -q get cpesel.sim.cur)
	_mgl=$(uci -q get cpesel.sim.gmval)

	[ -z "$_snum" ] && return 0

	if [ -n "$_mgl" ];then
		multi_gpio_led_control "$_mgl" "$_snum"
		return 0
	fi

	_devs="$gLedSim"
	[ -z "$_devs" ] && return 0

	_dev=$(init_dev "sim$_snum")

	for _tmp in $_devs; do
		if [ "$_dev" = "$_tmp" ]; then
			onoff_led "$_dev" 1
		else
			onoff_led "$_tmp" 0
		fi
	done
}

check_error() {
	local _dev="$1"
	local _wan="$2"
	local _lan="$3"

	if [ -z "$_dev" ] && [ -z "$gLedInt" ]; then
		return 0
	fi

	if [ -z "$_wan" ]; then
		check_lan "$_lan" || check_mesh
	else
		check_wan
	fi
}

check_cloud() {
	local _dev="$1"

	if [ -z "$_dev" ]; then
		return 0
	fi

	! ubus call mqttagent info|grep -sw "bridge"|grep -qsw "disconnected"
}

set_led() {
	local _dev=$1
	local _status=$2
	local _delay_on=$3
	local _delay_off=$4
	local _color=
	local _mode=

	_color=$(echo "$_dev"|cut -d':' -f2)
	_mode=$(echo "$_dev"|cut -d':' -f3)

	if [ -z "$_delay_off" ]; then
		_delay_off=$_delay_on
	fi

	case "$_status" in
	on)
		setled on "$_color" "$_mode"
		;;
	off)
		setled off "$_color" "$_mode"
		;;
	timer)
		setled timer "$_color" "$_mode" "$_delay_on" "$_delay_off"
		;;
	esac
}

onoff_led() {
	local _dev=$1
	local _status=$2
	local _action="on"
	local _line=""

	if [ -z "$_dev" ]; then
		return
	fi

	if [ "$_status" -eq 0 ]; then
		_action="off"
	fi

	for _line in $_dev; do
		set_led "$_line" "$_action"
	done
}

flash_check() {
	local _dev=$1
	local _num=

	_num=$(cat /sys/class/leds/hc\:blue\:sig*/brightness|grep -c 255)

	if [ "$_num" -gt 1 ] ;then
		onoff_led "$_dev" 0
		flash_leds "$_dev"
	fi
}

flash_leds() {
	local _dev=$1
	local _num=
	local _delay_on=
	local _delay_off=
	local _sleep_time=
	local _coefficient=200
	_num=$(echo "$_dev"|wc -l)

	_sleep_time=$(echo "$_num $_coefficient" | awk '{printf("%0.1f",$1*$2/1000)}')
	_delay_on=$((_num*_coefficient))
	_delay_off=$((_num*_coefficient*(_num-1)))

	for _line in $_dev; do
		echo "timer" > "$_line/trigger"
		echo "$_delay_on" > "$_line/delay_on"
		echo "$_delay_off" > "$_line/delay_off"
		async_sleep "$_sleep_time"
	done
}

fix_ctrl_code() {
	local _code=$1
	_code=$(echo "$_code" | awk '{printf "%d",$1}')

	if [ "$_code" -ge 4 ]; then
		echo 0x4
	elif [ "$_code" -ge 2 ]; then
		echo 0x2
	else
		echo 0x1
	fi
}

ctrl_led() {
	local _ctrl_code=$1
	local name=$2
	[ -z "$gLedMode" ] && _ctrl_code=$(fix_ctrl_code "$_ctrl_code")
	local _rsrp=$(get_sig_info "$name")
	[ -z "$_rsrp" ] && _rsrp="-120"

	if [ "$gIsMeshEnabled" = "1" ]; then
		# If Mesh disabled or Mesh success or Error Led not multi-use as Mesh Activity Led,
		# may need to control Error Led
		if [ "$gMeshStat" = "0" ] || \
			   [ "$gMeshStat" = "3" ] ||  \
			   [ "$gLedMeshAct" != "$gLedError" ]; then
			if [ "$gMeshStat" = "3" ] && \
				   [ "$gLedMeshAct" = "$gLedError" ]; then
				# If Mesh success and Error Led multi-use as Mesh Activity Led, turn off Error Led.
				onoff_led "$gLedError" 0
			elif [ "$gLedMeshAct" = "$gLedStatus" ] && [ "$gMeshStat" = 1 -o "$gMeshStat" = 2 ]; then
				# If Status Led multi-use as Mesh Activity Led(N6/N6Pro) and it's mesh connecting, turn off
				# Error Led.
				onoff_led "$gLedError" 0
			else
				onoff_led "$gLedError" $((!!(_ctrl_code&0x4)))
			fi
		fi
	else
		echo "_ctrl_code:$_ctrl_code"
		if [ "$gLock" != "1" ];then
			if [ -n "$gLedSig" ] ;then
				onoff_led "$gLedError" $((!!(_ctrl_code&0x4)))
			else
				if [ $((!!(_ctrl_code&0x4))) -eq 1 ];then
					set_led "$gLedError" "timer" "200" "200"
				else
					if [ "$gOnlineType" == "cpe" ];then
						if [ "$_rsrp" -lt -100 ]; then
							onoff_led "$gLedError" 1
						else
							onoff_led "$gLedError" 0
						fi
					else
						onoff_led "$gLedError" 0
					fi
				fi
			fi
		else
			set_led "$gLedError" "timer" "200" "200"
		fi
	fi

	onoff_led "$gLedCloud" $((!!(_ctrl_code&0x2)))

	if [ "$gIsMeshEnabled" = "1" ]; then
		# If it's controller or Mesh not success or Status Led not multi-use as Mesh Activity Led,
		# may need to control Status Led
		if [ "$gMeshRole" != "1" ] || \
			   [ "$gMeshStat" != "3" ] ||  \
			   [ "$gLedMeshAct" != "$gLedStatus" ]; then
			if [ "$gMeshRole" = "1" ] && [ "$gLedMeshAct" = "$gLedError" ]; then
				# If agent Error Led multi-use as Mesh Activity Led (N6/N6Pro) and it's mesh connecting, turn off
				# Status Led.
				if [ "$gMeshStat" != 3 ]; then
					if [ "$gHasWan" = "1" ] && [ "$((!!(_ctrl_code&0x1)))" = 1 ] && [ "$gMeshStat" != "1" ] && [ "$gMeshStat" != "2" ]; then
						onoff_led "$gLedStatus" 1
					else
						onoff_led "$gLedStatus" 0
					fi
				fi
			elif [ "$gLedMeshAct" = "$gLedStatus" ] && [ "$gMeshStat" != 1 -a "$gMeshStat" != 2 ]; then
				# If Status Led multi-use as Mesh Activity Led(N6/N6Pro) and it's not mesh connecting, check
				# Error Led status.
				onoff_led "$gLedStatus" $((!!(_ctrl_code&0x1)))
			elif [ "$gLedMeshAct" != "$gLedStatus" ]; then
				onoff_led "$gLedStatus" $((!!(_ctrl_code&0x1)))
			fi
		fi
	else
		if [ "$gLock" != "1" -o -n "$gLedCmode" -o -n "$gLedInt"  ];then
			onoff_led "$gLedStatus" $((!!(_ctrl_code&0x1)))
			if [ -n "$gLedSig" ] ;then
				onoff_led "$gLedStatus" $((!!(_ctrl_code&0x1)))
			else
				if [ $((!!(_ctrl_code&0x1))) -eq 1 ];then
					if [ "$gOnlineType" == "cpe" ];then
						if [ "$_rsrp" -lt -100 ]; then
							onoff_led "$gLedStatus" 0
						else
							onoff_led "$gLedStatus" 1
						fi
					else
						onoff_led "$gLedStatus" 1
					fi
				else
					onoff_led "$gLedStatus" 0
				fi
			fi
		else
			set_led "$gLedStatus" "timer" "200" "200"
		fi
	fi

	return 0
}

get_ctrl_code() {
	local _dev=$1
	local _code=

	_code=$(echo "$_dev" | sort | head -n1 | grep -oE "[a-z]+:0x[0-9a-fA-F]+:status[0-9]*" | cut -d':' -f2)

	echo "${_code:-0x1}"
}

set_ctrl_code() {
	local _code=$1
	local _hex=$2

	echo "$_code,$_hex" | awk -F, '{printf "0x%x\n",xor($1, $2)}'
}

check_schedule_time(){
	local ymd_s=$(date +%Y-%m-%d)
	local now_t=$(date +%s)
	local starttime
	local endtime
	if [ "$gScheduleStatus" != "1" ];then
		return 1
	fi

	if [ -n "$gScheduleStarttime" ];then
		starttime="${gScheduleStarttime}:00"
	else
		starttime="00:00:00"
	fi
	starttime_s=$(date -d "$ymd_s $starttime" +%s)
	if [ -n "$gScheduleEndtime" ];then
		endtime="${gScheduleEndtime}:00"
	else
		endtime="00:00:00"
	fi
	endtime_s=$(date -d "$ymd_s $endtime" +%s)

	if [ $starttime_s -ge $endtime_s ];then
		if [ $now_t -lt $endtime_s ];then
			return 0
		fi
		endtime_s=$((endtime_s+86400))
	fi

	if [ $now_t -gt $starttime_s -a $now_t -lt $endtime_s ];then
		return 0
	fi
	return 1
}

has_wan() {
	if uci -q get oem.feature.cpe|grep -sqE '[1-9]' \
		|| uci -q get network.nrswitch.nvlan|grep -sq W \
		|| uci -q get auto_adapt.mode.en|grep -sq 1 \
		|| uci -q get network.wisp.disabled|grep -sq 0; then
		echo 1
	fi
}

get_wan_list() {
	local cpe_cnt=$(uci -q get oem.feature.cpe)
	_list=$(uci show wanchk|grep network|sed "s/'//g"|sed "s/_.*//g"|awk -F= '{print $2}')
	_list="$_list wisp"
	if echo "$_list" |grep -sEq "wan$" ;then
		_list="$wan6 $_list"
	fi
	gWanList=""

	for _wan in $_list;do
		if echo "$_wan" |grep -sq "cpe" || echo "$_wan" |grep -sEq "wan[0-9]";then
			if [ $cpe_cnt -ge 1 ];then
				local background=$(uci -q get network.$_wan.background)
				[ "$background" == "1" ] && continue
				gWanList="$gWanList ${_wan} ${_wan}_6"
			fi			
		else
			gWanList="$gWanList ${_wan}"
		fi
	done
}

trigger_led() {
	[ "$gTrigger" = 1 ] && return
	gTrigger=1
	handle_led
}

trap "shutdown" USR2

shutdown(){
	logger -t "ledctrl" "shutdown led"
	gMode=1
	disable_all_leds
}

handle_led() {
	local status=
	local STAT= MODE= SIM= name= _fraction=
	[ "$gRunning" = 1 ] && return
	gRunning=1
	read -r gNow _fraction < /proc/uptime
	gNow=${gNow%%.*}
	status=$(ubus call ledctrl get|jsonfilter -e '@.status')
	if [ "$gMode" = "0" ]; then
		if [ "$status" = "off" ] || check_schedule_time;then
			disable_all_leds
		else
			gLock=$(ubus call infocd get "{\"name\":\"speedevent\"}"|jsonfilter -e '@.*[@.name="speedevent"]["parameter"]["speedevent_record"]["lock"]')

			gCtrlCode="$dCtrlCode"
			
			gIfaceData=$(ubus call network.interface dump)
			gWanData=$(cat /var/run/rpcd/wanchk.json 2>/dev/null)
			if ! check_error "$gLedError" "$gHasWan" "$gLanPorts"; then
				gCtrlCode=$(set_ctrl_code "$gCtrlCode" 0x4)
			fi

			if ! check_cloud "$gLedCloud"; then
				gCtrlCode=$(set_ctrl_code "$gCtrlCode" 0x2)
			fi
			check_model
			
			check_wifi

			if [ -n "$gLedSig" ] || [ -n "$gLedCmode" ] || [ -n "$gLedError" ];then
				update_cell_info
				name=$gCellName
				STAT=$gCellStat
				MODE=$gCellRadio
				SIM=$gCellSim
			fi

			check_cell_stat "$STAT" "$MODE" "$SIM" "$name"
			check_cur_sim
			if [ "$gIsMeshEnabled" = "1" ] && [ "$gForbidMesh" != "1" ]; then
				check_easy_mesh
			else
				check_wps
			fi

			ctrl_led "$gCtrlCode" "$name"
		fi
	fi
	gRunning=0
	if [ "$gTrigger" = 1 ]; then
		gTrigger=0
		handle_led
	fi
}

while getopts "w:m:h" opt; do
	case "${opt}" in
		w)
			gWait=${OPTARG}
			;;
		m)
			gMode=${OPTARG}
			;;
		h)
			usage
			;;
		\?)
			usage >&2
			exit 1
			;;
	esac
done

gLedStatus=$(init_dev "status")
gLedError=$(init_dev "error")
gLedCloud=$(init_dev "cloud")
gLedInt=$(init_dev "int")

gLanPorts=$(get_ports 'L')
gLedMode=$(uci -q get oem.board.led_mode)
gSigMode=$(uci -q get oem.board.sig_mode)
gMeshRole=$(uci -q get mesh.config.role)
gMeshRole=${gMeshRole:-1}

init_mesh_dev

dCtrlCode=$(get_ctrl_code "$gLedStatus")

gHasWan=$(has_wan)

gIsMeshEnabled=$(uci -q get mesh.config.enabled)
gLedWifi=$(init_dev "wifi")
gLedSim=$(init_dev "sim")
gLedSig1=$(init_dev "sig1")
gLedSig2=$(init_dev "sig2")
gLedSig3=$(init_dev "sig3")

gLedSig=$(init_dev "sig")
gLedCmode=$(init_dev "cmode")
gLedCmode5=$(init_dev "cmode5")
gLedCmode4=$(init_dev "cmode4")

gScheduleStatus=$(uci -q get ledctrl.ledctrl.schedule)
gScheduleStarttime=$(uci -q get ledctrl.ledctrl.starttime)
gScheduleEndtime=$(uci -q get ledctrl.ledctrl.endtime)

if [ "$gMode" = "1" ] || check_schedule_time; then
	disable_all_leds
fi

while true; do
	handle_led
	async_sleep "$gWait"
done
