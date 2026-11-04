#!/bin/sh

ACTION=$1

url="https://static.qiyou.cn/upgrade/ljb/plugin/qyplug.sh"
sh_file="/tmp/qyplug.sh"
ret_file="/tmp/qyplug.ret"
get_file="/tmp/qyplug.get"
pid_file="/tmp/qyplug.pid"
#get_qyplug="wget -q -T 30 \"${url}\" -O \"${sh_file}\" --no-check-certificate"
get_qyplug="curl -s -k -L -m 30 \"${url}\" -o \"${sh_file}\""

rules_pid="/var/run/qiyougamebooster-rules.pid"

# Reuse the device selectors and split policy owned by the running Qiyou core.
# Each HOST_HOOK table disappears when its device stops acceleration.
rules_sync(){
	local tables table load nat selector handle forward index mark wanted= mangle have reorder
	tables=$(nft list tables 2>/dev/null | awk '$2 == "ip" && $3 ~ /^HOST_HOOK[0-9]+$/ { print $3 }')
	for table in $tables; do
		load=$(nft list chain ip "$table" ACC_LOAD 2>/dev/null) || continue
		nat=$(nft -a list chain ip "$table" ACC_NAT_SPLIT 2>/dev/null) || continue
		selector=$(printf '%s\n' "$load" | awk '/goto ACC_LOAD_SPLIT/ { sub(/^[ \t]+/, ""); sub(/ counter.*$/, ""); print }')
		[ -n "$selector" ] || continue
		index=$(printf '%s' "$table" | sed 's/^HOST_HOOK0*//')
		mark=$(sed -n 's/^define acc_mark = \(0x[0-9a-fA-F]*\)$/\1/p' "/tmp/qiyou/acc_ipt.$index" 2>/dev/null)
		if [ -n "$mark" ]; then
			mark=$(printf '0x%08x' "$mark")
			wanted="$wanted
$(printf '%s\n' "$selector" | while IFS= read -r rule; do
	printf '%s meta l4proto udp meta mark & 0x000000ff == %s counter return comment "qy_acc udp tunnel %s"\n' "$rule" "$mark" "$table"
done)"
		fi
		# Preserve Qiyou's original TCP target and policy route after its DNATs.
		case "$nat" in
		*'comment "qy_acc tcp binding"'*) ;;
		*)
			handle=$(printf '%s\n' "$nat" | awk '/accept # handle/ { print $NF; exit }')
			[ -z "$handle" ] || nft insert rule ip "$table" ACC_NAT_SPLIT position "$handle" meta l4proto tcp fib daddr type != local counter dnat ip to ip daddr comment '"qy_acc tcp binding"'
			;;
		esac
	done

	# Qiyou marks its own selected UDP packets before the shared mangle hook.
	# Leave those packets on Qiyou's native TUN route; do not assign a second
	# transparent-proxy socket. Only rules owned by Qiyou are maintained here.
	mangle=$(nft -a list chain inet fw4 mangle_prerouting 2>/dev/null)
	wanted=$(printf '%s\n' "$wanted" | sed '/^$/d' | sort)
	have=$(printf '%s\n' "$mangle" | awk '/comment "qy_acc udp tunnel / {
		sub(/^[ \t]+/, ""); sub(/ # handle.*$/, "")
		sub(/counter packets [0-9]+ bytes [0-9]+/, "counter"); print
	}' | sort)
	reorder=$(printf '%s\n' "$mangle" | awk '/ # handle/ && !/chain mangle_prerouting/ {
		if (/comment "qy_acc udp tunnel /) { if (other) move = 1 }
		else other = 1
	} END { print move + 0 }')
	if [ -n "$mangle" ] && { [ "$have" != "$wanted" ] || [ "$reorder" -ne 0 ]; }; then
		{
			printf '%s\n' "$mangle" | awk '/comment "qy_acc udp tunnel / { print "delete rule inet fw4 mangle_prerouting handle " $NF }'
			printf '%s\n' "$wanted" | while IFS= read -r rule; do
				[ -z "$rule" ] || printf 'insert rule inet fw4 mangle_prerouting %s\n' "$rule"
			done
		} | nft -f -
	fi

	# Keep Qiyou's existing tunnel accepts ahead of generic forward rules.
	forward=$(nft -a list chain inet fw4 forward 2>/dev/null) || return
	printf '%s\n' "$forward" | awk '
		/ # handle/ && !/chain forward/ {
			if (/comment "qy_acc"/ && /[io]ifname "tun3\*" accept/) {
				h[++n] = $NF
				sub(/^[ \t]+/, ""); sub(/ # handle.*$/, ""); r[n] = $0
				if (other) move = 1
			} else other = 1
		}
		END { if (move) for (i = 1; i <= n; i++) {
			print "delete rule inet fw4 forward handle " h[i]
			print "insert rule inet fw4 forward " r[i]
		} }
	' | nft -f -
}

rules_stop(){
	local pid children command
	pid=$(cat "$rules_pid" 2>/dev/null)
	if [ -n "$pid" ] && [ "$pid" != "$$" ]; then
		command=$(tr '\0' ' ' < "/proc/$pid/cmdline" 2>/dev/null)
		case "$command" in
		*'/usr/bin/qiyougamebooster.sh '* )
			children=$(pgrep -P "$pid")
			kill $children "$pid" 2>/dev/null
			;;
		esac
	fi
	rm -f "$rules_pid"
}

rules_watch(){
	rules_stop
	echo $$ > "$rules_pid"
	trap 'kill $(pgrep -P $$) 2>/dev/null; [ "$(cat "$rules_pid" 2>/dev/null)" != "$$" ] || rm -f "$rules_pid"' EXIT
	trap 'exit 0' TERM INT
	rules_sync
	nft monitor | while IFS= read -r event; do
		case "$event" in
		*'rule ip HOST_HOOK'*|*'table ip HOST_HOOK'*|*'table inet fw4'*|*'rule inet fw4 mangle_prerouting '*|*'rule inet fw4 forward '*) rules_sync ;;
		esac
	done &
	wait "$!"
}

stop(){
	rules_stop
	pid=`cat ${get_file} 2> /dev/null`
	echo "qy_acc stop: ${pid}"
	[ "${pid}" = "" ] || kill -9 ${pid}
	rm -rf ${get_file}

	sleep 1

	if [ -e /tmp/qyplug.sh ]; then
		sh /tmp/qyplug.sh stop
		rm -rf /tmp/qyplug.sh
	fi

	sleep 2
	rm -rf ${ret_file}

	if [ -e /tmp/qy ]; then
		cd /tmp/qy && ./init.sh stop
		rm -rf /tmp/qy
	fi
}

start(){
	pid=$$
	echo ${pid} > ${get_file}
	echo "qy_acc start: ${pid}"

	while true
	do
		rm -rf ${sh_file} ${ret_file}

		sh -c "${get_qyplug}"
		if [ -e ${sh_file} ]; then
			export QY_NOT_INSTALL=1
			sh ${sh_file} &
		fi

		sleep 3
		[ ! -e ${ret_file} ] || break
		sleep 30
	done

	rm -rf ${get_file}
}

switch(){
	act=$1

	if [ ! -d /tmp/qy ]; then
		echo "NOT ENABLED"
		return
	fi

	if [ "${act}" = "on" ]; then
		cd /tmp/qy && ./init.sh &> /dev/null
	elif [ "${act}" = "off" ]; then
		cd /tmp/qy && ./init.sh stop &> /dev/null
	fi
}

status(){
	status=`cat ${ret_file} 2> /dev/null`
	get=`cat ${get_file} 2> /dev/null`
	pid=`cat ${pid_file} 2> /dev/null`
	acc=`pidof qy_acc 2> /dev/null`

	if [ "${status}" = "succeeded" ]; then
		if [ ! -d /tmp/qy ]; then
			printf "NOT ENABLED"
		elif [ "${acc}" = "" ]; then
			printf "CLOSED"
		elif [ -d /sys/class/net/tun31 ] || [ -d /sys/class/net/tun32 ]; then
			printf "BOOSTING"
		else
			printf "RUNNING"
		fi
		return
	fi
	if [ "${status}" = "notsupport" ]; then
		printf "NOT SUPPORTED"
		return
	fi
	if [ "${status}" = "getpkgerr" ]; then
		printf "DOWNLOADING"
		return
	fi
	if [ "${get}" != "" ] && [ -e "/proc/${get}" ]; then
		printf "INSTALLING"
		return
	fi
	if [ "${pid}" != "" ] && [ -e "/proc/${pid}" ]; then
		printf "INSTALLING"
		return
	fi
	if [ ! -d /tmp/qy ]; then
		printf "NOT ENABLED"
		return
	fi
	printf "NOT RUNNING"
}

version(){
	ver=`sed -n 's;^VERSION=;;p' /tmp/qy/etc/PKG_INFO 2> /dev/null`
	printf "${ver}"
}

case $ACTION in
stop)
	stop
	;;
start)
	stop
	start
	rules_watch
	;;
rules)
	rules_sync
	;;
watch)
	rules_watch
	;;
status)
	status
	;;
enable)
	switch on
	;;
disable)
	switch off
	;;
version)
	version
	;;
*)
	start
	rules_watch
	;;
esac
