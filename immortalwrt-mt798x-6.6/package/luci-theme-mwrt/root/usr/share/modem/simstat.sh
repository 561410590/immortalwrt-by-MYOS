#!/bin/sh
# 供 LuCI 轮询的 SIM 状态接口,输出 JSON。
#
# 切卡脚本 mt5700m.sh 跑一趟要 30 秒左右(含 sleep 11 + CFUN 循环 + chkSimExt),
# 它运行期间锁文件 /tmp/rm520n.lock 存在。这段时间本脚本不发任何 AT ——
# 一是别跟切卡脚本抢 AT 口,二是切换过程中的状态码没有参考价值,
# 直接回报"切换中"让界面显示进度即可。
#
# 输出的四个字段都是受控值(数字/中文/十六进制),不含引号和反斜杠,
# 故不做 JSON 转义(BusyBox sed 对转义表达式支持也不全)。

LOCK=/tmp/rm520n.lock

SEL=$(uci -q get mwrt.@ndis[0].simsel)
# 机型判定:三卡槽(外置SIM1/外置SIM2/内置卡)是 C2000 系列专属的硬件布局。
# 其余机型(H5000M 等)物理上只有一个卡槽,不论 simsel 取值一律显示"机身卡槽",
# 与"卡槽选择"下拉框保持一致,不暴露本机不存在的卡槽。判据与 CBI/页脚一致:含 c2000。
IDSTR=$(cat /tmp/sysinfo/model /tmp/sysinfo/board_name 2>/dev/null | tr 'A-Z' 'a-z')
case "$IDSTR" in
    *c2000*) IS_C2000=1 ;;
    *)       IS_C2000=0 ;;
esac
if [ "$IS_C2000" = "1" ]; then
    case "$SEL" in
        6)   SLOT="外置SIM2卡槽" ;;
        0)   SLOT="外置SIM1卡槽" ;;
        1|2) SLOT="内置卡" ;;
        *)   SLOT="未知($SEL)" ;;
    esac
else
    SLOT="机身卡槽"
fi

if [ -e "$LOCK" ]; then
    PID=$(cat "$LOCK" 2>/dev/null)
    if [ -n "$PID" ] && [ -d "/proc/$PID" ]; then
        printf '{"busy":1,"code":"","text":"正在切换卡槽,请稍候…","slot":"%s","iccid":""}\n' "$SLOT"
        exit 0
    fi
fi

SQ=$(sendat 1 'AT^SIMSQ?' 2>/dev/null | tr -d '\r' | sed -n 's/^\^SIMSQ: *//p' | head -1)
CODE=$(echo "$SQ" | cut -d, -f2)

# 状态码含义与 mt5700m.sh 的 chkSimExt() 保持一致
case "$CODE" in
    0)   TEXT="SIM卡未插入" ;;
    1)   TEXT="SIM卡已插入" ;;
    2)   TEXT="SIM卡被锁" ;;
    3)   TEXT="SIMLOCK 锁定" ;;
    10)  TEXT="卡文件正在初始化" ;;
    11)  TEXT="卡初始化完成(可接入网络)" ;;
    12)  TEXT="SIM卡正常工作" ;;
    98)  TEXT="卡物理失效(PUK锁死或卡损坏)" ;;
    99)  TEXT="卡已移除" ;;
    100) TEXT="卡错误(初始化失败)" ;;
    "")  TEXT="未读到状态(模组无响应)" ;;
    *)   TEXT="未知状态码 $CODE,请参考 AT 手册" ;;
esac

# 只在确实有卡时才多发一条 AT,避免空槽时白白占用 AT 口
ICCID=""
case "$CODE" in
    1|11|12) ICCID=$(sendat 1 'AT^ICCID?' 2>/dev/null | tr -d '\r' | sed -n 's/.*ICCID: *//p' | head -1) ;;
esac

# 顺带刷新这个文件,其他脚本还在读它
echo "$TEXT" > /tmp/simcardstat

printf '{"busy":0,"code":"%s","text":"%s","slot":"%s","iccid":"%s"}\n' \
    "$CODE" "$TEXT" "$SLOT" "$ICCID"