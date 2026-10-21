#!/bin/sh
# runtime/uninstall-runtime.sh — 部署到设备端的卸载脚本
# 安装时由 install.sh 重命名为 leigod_uninstall.sh 放到 work_dir。
# 自适应平台：根据 acc_firewall.ini 位置与 init 机制识别平台并清理。
#
# 卸载顺序（网络安全优先）：
#   1. 停 watchdog（cron/procd/自启脚本/monitor 进程）——防止清理期间 acc 被重新拉起
#   2. 清防火墙规则——规则一删，被加速设备流量立即回归正常路由
#   3. 停加速进程——此时规则已清，进程即使被强杀也不会留下"流量重定向到已死代理"的断网窗口
#   4. 删文件
# 关键点：规则必须先于进程清理。若先杀进程再清规则，进程已死但 iptables/nft 规则仍把
# 流量重定向到已删除的代理端口，会导致被加速设备在卸载中途断网（用户反馈的问题）。

BINARY_PREFIX="acc-gw.router"
ACC_LOG="/tmp/leigod_acc_uninstall.log"

ulog() {
    printf '[%s] %s\n' "$(date '+%H:%M:%S')" "$*" >> "${ACC_LOG}" 2>/dev/null
    echo "$*"
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

# 遍历 /proc，返回所有 cmdline 含 <pattern> 的 pid。
# 不用 pidof：acc-gw.router.<arch> 与 acc_upgrade_monitor 均为 19 字符，超过内核
# comm 的 15 字符限制被截断（如 acc-gw.router.a），BusyBox pidof 按 comm 全名匹配
# 会永久找不到 → 漏杀。cmdline 不截断，匹配可靠。
find_pids() {
    local pattern="$1" p cmdline
    for p in /proc/[0-9]*; do
        [ -r "${p}/cmdline" ] || continue
        cmdline=$(tr '\0' ' ' < "${p}/cmdline" 2>/dev/null) || continue
        case "${cmdline}" in
            *"${pattern}"*) basename "${p}" ;;
        esac
    done
}

# 停止所有自启/守护源，防止清理期间 monitor/cron/procd 把 acc 重新拉起。
# 必须在最前面完成：小米 cron 每分钟触发，若不先摘 cron 条目，杀完 monitor 后
# 下一分钟 cron 又会拉起 monitor → 重新拉起 acc，与卸载流程竞争。
stop_watchdog() {
    # cron 守护（小米）
    if [ -f /etc/crontabs/root ]; then
        sed -i '/leigod\/monitor\.sh/d' /etc/crontabs/root 2>/dev/null
        /etc/init.d/cron restart >/dev/null 2>&1 || killall -HUP crond 2>/dev/null
    fi
    # procd init（OpenWrt）：先 disable 阻止 respawn
    if [ -f /etc/init.d/acc ]; then
        /etc/init.d/acc disable >/dev/null 2>&1
    fi
    # 华硕自启脚本
    for idir in /koolshare/init.d /jffs/softcenter/init.d /jffs/scripts; do
        rm -f "${idir}/S99LeigodAcc.sh" 2>/dev/null
    done
    # 停 monitor 守护进程本身
    local p
    for p in $(find_pids "monitor.sh"); do
        kill -9 "${p}" >/dev/null 2>&1
    done
    ulog "已停止守护/自启"
}

# 完整清理加速防火墙规则（与引擎 IPTablesMgr/NftableMgr 对齐）。
# 必须在 kill 进程之前执行：规则删除后流量立即回正常路由，消除断网窗口。
clean_rules() {
    local chain="GAMEACC" tbl built name

    # iptables：引擎在 mangle/filter/nat 三张表均建 GAMEACC 链，并从内置链
    # (mangle PREROUTING / filter INPUT / nat PREROUTING) 跳入。-X 删链有两个硬前提：
    # 链内无规则(-F)、无跳转引用(-D)，故每表固定三步，顺序不能乱。
    for tbl in mangle filter nat; do
        case "${tbl}" in filter) built="INPUT" ;; *) built="PREROUTING" ;; esac
        iptables -t ${tbl} -D ${built} -j ${chain} 2>/dev/null
        iptables -t ${tbl} -F ${chain} 2>/dev/null
        iptables -t ${tbl} -X ${chain} 2>/dev/null
    done

    # nft：引擎建表 inet tproxy_table。一条命令删整个表（含所有链和规则）。
    # 绝不能用 `nft flush ruleset`（会清空整机全部 nft 规则，破坏路由器自身防火墙）。
    nft delete table inet tproxy_table 2>/dev/null

    # ipset：引擎按 direct_/target_/proxy_ 前缀建集合（如 direct_Phone）。
    # 规则已删后集合无人引用，残留无害但一并清理；仅删匹配前缀的，避免误删用户/系统集合。
    for name in $(ipset list -n 2>/dev/null); do
        case "${name}" in
            direct_*|target_*|proxy_*) ipset destroy "${name}" 2>/dev/null ;;
        esac
    done

    ulog "已清理防火墙规则"
}

# 停加速进程。先 SIGUSR1 优雅退出（引擎 signal_handler 置 g_is_stop，走 stop()
# 关闭代理会话），再 kill -9 兜底残留。引擎 SIGTERM 处理已注释，故不依赖 TERM。
kill_acc() {
    local arch pat p pids
    arch=$(get_arch)
    # 优雅退出
    for pat in "${BINARY_PREFIX}.${arch}" "acc_upgrade_monitor"; do
        for p in $(find_pids "${pat}"); do
            kill -USR1 "${p}" >/dev/null 2>&1
        done
    done
    sleep 1
    # 兜底强杀残留
    for pat in "${BINARY_PREFIX}.${arch}" "acc_upgrade_monitor"; do
        for p in $(find_pids "${pat}"); do
            kill -9 "${p}" >/dev/null 2>&1
        done
    done
    ulog "已停止加速进程"
}

# 删除文件（按平台识别）
delete_files() {
    local work_dir

    # 华硕：$work_dir/config/acc_firewall.ini（与引擎 Board.cpp 路径对齐）
    for d in /jffs/.koolshare/leigod/acc /jffs/softcenter/leigod/acc /jffs/leigod/acc /koolshare/leigod/acc; do
        if [ -f "${d}/config/acc_firewall.ini" ] || [ -d "${d}" ]; then
            work_dir="${d}"
            ulog "识别为华硕平台"
            rm -rf "${work_dir}"
            return 0
        fi
    done

    # 小米
    if [ -d /userdisk/appdata/leigod ]; then
        ulog "识别为小米平台"
        rm -rf /userdisk/appdata/leigod
        rm -f /etc/config/accelerator.ini /etc/config/acc_firewall.ini /etc/config/acc_version.ini \
              /etc/config/acc_core_conf.json /etc/config/ipdatacloud_country.xdb
        return 0
    fi

    # OpenWrt 通用
    if [ -d /usr/sbin/leigod ] || [ -f /etc/init.d/acc ]; then
        ulog "识别为 OpenWrt 平台"
        rm -f /etc/init.d/acc
        rm -rf /usr/sbin/leigod
        rm -f /etc/config/accelerator.ini /etc/config/acc_firewall.ini /etc/config/acc_version.ini \
              /etc/config/acc_core_conf.json /etc/config/ipdatacloud_country.xdb
        return 0
    fi

    ulog "未识别到已安装的加速插件"
    return 1
}

ulog "开始卸载雷神加速插件"
stop_watchdog
clean_rules
kill_acc
clean_rules   # 兜底：防止清理→杀进程的瞬间被拉起的进程残留规则
delete_files
ulog "卸载完成"
