#!/bin/sh
# runtime/monitor.sh — 统一守护脚本（替代 plugin_monitor.sh + plugin_asus_monitor.sh）
# 职责（设计6）：每轮确保内核模块已加载 + 进程存活补拉 + upgrade monitor 存活。
# 自适应平台：根据 acc_firewall.ini 位置定位工作目录，读 mode 决定模块列表。
# 单例锁防止重复运行。

WORK_DIR="$(cd "$(dirname "$0")" 2>/dev/null && pwd)"
LOCK_FILE="${WORK_DIR}/acc_daemon.monitor.lock"
MONITOR_LOG="/tmp/leigod_acc_monitor.log"
BINARY_PREFIX="acc-gw.router"
INTERVAL=${MONITOR_INTERVAL:-10}

# nohup 检测：小米等精简 BusyBox 无 nohup。monitor 由 cron/procd 拉起、已脱离终端，
# daemon 子进程不会收到 SIGHUP，故无 nohup 时直接后台执行即可（与 acc-asus.sh 同思路）
NOHUP=""
type nohup >/dev/null 2>&1 && NOHUP="nohup"

monitor_log() {
    printf '[%s] %s\n' "$(date '+%H:%M:%S')" "$*" >> "${MONITOR_LOG}" 2>/dev/null
}

get_arch() {
    local raw
    raw=$(uname -m 2>/dev/null)
    case "${raw}" in
        x86_64) echo "amd64" ;;
        aarch64|aarch64_be) echo "arm64" ;;
        mips|mips64) echo "mipsel" ;;
        armv7l|armv7hl|armv6l|armv5tel) echo "arm" ;;
        *) echo "${raw}" ;;
    esac
}

# 定位 acc_firewall.ini（openwrt/xiaomi→/etc/config，asus→$WORK_DIR/config）
find_firewall_ini() {
    if [ -f "${WORK_DIR}/config/acc_firewall.ini" ]; then
        echo "${WORK_DIR}/config/acc_firewall.ini"
    elif [ -f /etc/config/acc_firewall.ini ]; then
        echo "/etc/config/acc_firewall.ini"
    fi
    return 0
}

read_field() {
    local field="$1" cfg
    cfg=$(find_firewall_ini)
    [ -n "${cfg}" ] && [ -f "${cfg}" ] || return 1
    grep "^${field}=" "${cfg}" | head -1 | cut -d'=' -f2-
}

# 设计6：确保内核模块已加载（按 mode）
ensure_modules_loaded() {
    local mode modules m
    mode=$(read_field mode 2>/dev/null)
    [ -n "${mode}" ] || mode="tproxy"

    if [ "${mode}" = "tproxy" ]; then
        modules="xt_TPROXY nf_tproxy_core xt_set ip_set ip_set_hash_net ip_set_list_set"
    elif [ -e /dev/net/tun ]; then
        # tun 内建进内核的固件（如小米）：设备节点存在即可用，无需 modprobe
        modules=""
    else
        modules="tun"
    fi

    for m in ${modules}; do
        if ! lsmod 2>/dev/null | grep -q "^${m} "; then
            modprobe "${m}" >/dev/null 2>&1 || monitor_log "modprobe ${m} 失败"
        fi
    done
}

# 进程查找统一基于 /proc/<pid>/cmdline，不依赖 pidof：
# acc-gw.router.<arch> 与 acc_upgrade_monitor 进程名均 19 字符，超过 Linux 内核
# comm 的 15 字符限制被截断（如 acc-gw.router.a），BusyBox pidof 按 comm 全名匹配
# 会永久找不到 → 误判进程未运行 → 每 10 秒重复拉起。cmdline 不截断，匹配可靠。

# 遍历 /proc，返回首个 cmdline 含 <pattern> 的 pid（固定字符串匹配）
find_proc() {
    local pattern="$1" p
    for p in /proc/[0-9]*; do
        [ -r "${p}/cmdline" ] || continue
        if tr '\0' ' ' < "${p}/cmdline" 2>/dev/null | grep -qF -- "${pattern}"; then
            basename "${p}"
            return 0
        fi
    done
    return 1
}

# 进程是否存活（cmdline 含进程名即可）
process_alive() {
    find_proc "$1" >/dev/null 2>&1
}

# 精确匹配进程：cmdline 同时含进程名与参数模式（区分 -r daemon / -r web，避免误判）
# daemon 和 web 是同一二进制，须看 cmdline 里的 -r 参数
is_process_running() {
    local name="$1" pattern="$2" p cmdline
    for p in /proc/[0-9]*; do
        [ -r "${p}/cmdline" ] || continue
        cmdline=$(tr '\0' ' ' < "${p}/cmdline" 2>/dev/null) || continue
        case "${cmdline}" in
            *"${name}"*) echo "${cmdline}" | grep -qF -- "${pattern}" && return 0 ;;
        esac
    done
    return 1
}

# 拉起 acc 进程（读 mode 拼 -m）
start_acc() {
    local mode tproxy_ip args arch
    mode=$(read_field mode 2>/dev/null)
    tproxy_ip=$(read_field tproxy_ip 2>/dev/null)
    [ -n "${mode}" ] || { monitor_log "拉起失败: 无 mode"; return 1; }
    arch=$(get_arch)

    args="-r daemon -m ${mode} -p 5588"
    [ "${mode}" = "tproxy" ] && args="${args} -l ${tproxy_ip:-10.20.30.40}"

    monitor_log "拉起 acc: ${WORK_DIR}/${BINARY_PREFIX}.${arch} ${args}"
    # 输出保留到独立日志（非 /dev/null），便于排查进程启动即退出的真崩溃
    ${NOHUP} "${WORK_DIR}/${BINARY_PREFIX}.${arch}" ${args} >>"${WORK_DIR}/acc_daemon.out" 2>&1 &
}

start_upgrade_monitor() {
    local bin="${WORK_DIR}/acc_upgrade_monitor"
    [ -f "${bin}" ] || return 0
    ${NOHUP} "${bin}" -r upgrade >>"${WORK_DIR}/acc_upgrade_monitor.out" 2>&1 &
    monitor_log "拉起 acc_upgrade_monitor"
}

# 单例锁
if [ -f "${LOCK_FILE}" ]; then
    if kill -0 "$(cat "${LOCK_FILE}" 2>/dev/null)" 2>/dev/null; then
        monitor_log "monitor 已在运行，退出"
        exit 1
    fi
    rm -f "${LOCK_FILE}"
fi
echo $$ > "${LOCK_FILE}"
trap 'rm -f "${LOCK_FILE}" 2>/dev/null' INT TERM EXIT

ARCH=$(get_arch)
ACC_BIN="${BINARY_PREFIX}.${ARCH}"

monitor_log "monitor 启动, work=${WORK_DIR}, bin=${ACC_BIN}"

# 主循环
while true; do
    # 设计6：每轮先确保模块已加载
    ensure_modules_loaded

    # daemon 进程补拉（精确匹配 -r daemon；web 进程由 daemon 自行管理）
    # 不能用 pidof ACC_BIN：daemon/web 同名，杀掉 daemon 而 web 还在时 pidof 仍非空会误判
    if ! is_process_running "${ACC_BIN}" "-r daemon"; then
        monitor_log "daemon 进程未运行，拉起"
        start_acc
        sleep 3
    fi

    # upgrade monitor 补拉
    if ! process_alive acc_upgrade_monitor; then
        start_upgrade_monitor
    fi

    sleep "${INTERVAL}"
done
