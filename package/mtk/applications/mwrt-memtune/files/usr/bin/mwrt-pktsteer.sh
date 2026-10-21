#!/bin/sh
# Mwrt 数据包分流 (A 方案) —— 按在线核数自适应,双核/四核通用
#   1) WiFi WED 中断从 CPU0 挪到"最后一个在线核"(双核=CPU1, 四核=CPU3),给 CPU0 让路
#   2) 所有 WiFi/以太 rx 队列的 RPS 摊到全部在线核,让协议栈软中断不堆在中断核上
#   3) 打开 RFS(rps_sock_flow_entries),让同一条流尽量落回处理它的核,减少乱序
#
# 纯运行时网络调优,不碰任何存储/官方分区,任意机型安全;重复执行幂等。

# ---- 在线核数与掩码(十六进制,不带 0x)----
ncpu=$(grep -c '^processor' /proc/cpuinfo 2>/dev/null)
[ "$ncpu" -ge 1 ] 2>/dev/null || ncpu=1
allmask=$(printf '%x' $(( (1 << ncpu) - 1 )))      # 全核: 双核=3, 四核=f
lastmask=$(printf '%x' $(( 1 << (ncpu - 1) )))     # 末核: 双核=2, 四核=8

# ---- 1) WED 中断 -> 最后一个核(单核则原地,无害)----
if [ "$ncpu" -gt 1 ]; then
	for irq in $(awk -F: 'tolower($0) ~ /wed/{gsub(/[^0-9]/,"",$1); if($1!="") print $1}' /proc/interrupts); do
		echo "$lastmask" > "/proc/irq/$irq/smp_affinity" 2>/dev/null
	done
fi

# ---- 2) RPS: WiFi + eth 的 rx 队列摊到全核 ----
for q in /sys/class/net/*/queues/rx-*/rps_cpus; do
	[ -e "$q" ] || continue
	dev=$(echo "$q" | sed -n 's#/sys/class/net/\([^/]*\)/.*#\1#p')
	case "$dev" in
		ra*|rai*|apcli*|eth*)
			echo "$allmask" > "$q" 2>/dev/null
			echo 256 > "$(dirname "$q")/rps_flow_cnt" 2>/dev/null
			;;
	esac
done

# ---- 3) RFS 全局流表 ----
echo 4096 > /proc/sys/net/core/rps_sock_flow_entries 2>/dev/null

exit 0
