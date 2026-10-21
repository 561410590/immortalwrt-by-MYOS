#!/bin/bash
LOCKFILE="/tmp/zinfom5700-AK68.lock"
cleanup() {
    # Keep the flock inode stable. Descriptor 200 releases it on process exit.
    :
}
{
    flock -n 200 || {
        echo "脚本已经运行..."
        exit 1
    }
    echo "Script is running..."
    trap cleanup EXIT
    trap 'exit 0' INT TERM
    echo "running..."
    echo "running..." > /tmp/stsss.file
    echo "初始化TCP AT程序..." > /tmp/stsss.file
    echo "初始化TCP AT程序..."
    #sleep 3


    FILE138="/tmp/modconf-AK68.conf"
    if [ ! -f "$FILE138" ]; then
    	echo "文件不存在: $FILE138"
    	cleanup
    fi

    SIM_Check=$(atsd_tools_cli -i cpe -c "AT^CARDMODE")
    CARD_MODE=$(printf '%s\n' "$SIM_Check" | tr -d '\r' | sed -n 's/^[[:space:]]*\^CARDMODE:[[:space:]]*\([0-9][0-9]*\).*/\1/p' | head -n 1)
    if [ "$CARD_MODE" = "0" ]; then
        {    
        echo '未检测到信息'
        echo `atsd_tools_cli -i cpe -c "ATI" | sed -n '3p'|sed 's/\r$//'` #'RM520N-CN'
        echo `atsd_tools_cli -i cpe -c "ATI" | sed -n '2p'|sed 's/\r$//'` #'Quectel'
        echo `date "+%Y-%m-%d %H:%M:%S"`
        echo ''
        echo "未检测到SIM卡!"
        echo -e "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n"
        } > /tmp/cpe_cell-AK68.file.new
        mv /tmp/cpe_cell-AK68.file.new /tmp/cpe_cell-AK68.file
        exit
    fi


    Temperature(){
        #OX=$(atsd_tools_cli -i cpe -c 'AT^CHIPTEMP?'| grep 'CHIPTEMP' | sed -n '1p' | cut -d, -f9 | sed '/^$/d')
        OX=$(atsd_tools_cli -i cpe -c 'AT^CHIPTEMP?' | tr -d '\r' \
            | sed -n 's/^[[:space:]]*\^CHIPTEMP:[[:space:]]*//p' | cut -d, -f1 | head -n 1)
        case "$OX" in
            ''|*[!0-9]*) CTEMP="-" ;;
            *) CTEMP=$((OX / 10)) ;;
        esac
    }
    ISP_Read(){
            operatorCode=$(atsd_tools_cli -i cpe -c 'AT^EONS=2' | tr -d '\r' \
                | sed -n 's/^[[:space:]]*\^EONS:[[:space:]]*//p' | cut -d, -f2 | head -n 1)
            case "$operatorCode" in
            "46000" | "46002" | "46004" | "46007" | "46008" | "46020")
                ISP="中国移动"
                ;;
            "46001" | "46006" | "46009")
                ISP="中国联通"
                ;;
            "46003" | "46005" | "46011")
                ISP="中国电信"
                ;;
            "46015")
                ISP="中国广电"
                ;;
            *)
                ISP="未知运营商"
                ;;
            esac
    }

    TD_Tech_Ltd_SIMINFO()
    {
        # 获取IMEI
        IMEI=$(atsd_tools_cli -i cpe -c "AT+CGSN" | tr -d '\r' | grep -E '^[0-9]{15}$' | head -n 1)
        # 获取载波聚合数据
        zbjhox=$(5700_get_zbjh-AK68.sh)
        if [ "$zbjhox" == "" ] ; then
            Zbjh="Loss 无载波聚合"
        else
            Zbjh=$zbjhox
        fi
        # 获取IMSI
        IMSI=$(atsd_tools_cli -i cpe -c "AT+CIMI" | tr -d '\r' | grep -E '^[0-9]{14,16}$' | head -n 1)
        # 获取ICCID
        ICCID=$(atsd_tools_cli -i cpe -c "AT^ICCID?" | tr -d '\r' \
            | sed -n 's/^[[:space:]]*\^ICCID:[[:space:]]*//p' | head -n 1)
        # 获取电话号码
        phone=$(atsd_tools_cli -i cpe -c "AT+CNUM"  | awk -F ',' '{print $2}'| sed '/^$/d' )
        phone=${phone:1:-1}
        QCI1=$(atsd_tools_cli -i cpe -c 'AT+CGEQOSRDP=8' | tr -d '\r' \
            | sed -n 's/^[[:space:]]*+CGEQOSRDP:[[:space:]]*//p' | head -n 1)

        QCI2=$(echo $QCI1 | cut -d ',' -f2)
        if [[ "$QCI2" =~ "1" ]]; then
            QCI="QCI值：1-GBR优级2\\-Budget100ms"
        elif [[ "$QCI2" =~ "2" ]]; then
            QCI="QCI值：2-GBR优级4\\-Budget150ms"
        elif [[ "$QCI2" =~ "3" ]]; then
            QCI="QCI值：3-GBR优级3\\-Budget50ms"
        elif [[ "$QCI2" =~ "4" ]]; then
            QCI="QCI值：4-GBR优级5\\-Budget300ms"
        elif [[ "$QCI2" =~ "5" ]]; then
            QCI="QCI值：5-GBR优级1\\-Budget100ms"
        elif [[ "$QCI2" =~ "6" ]]; then
            QCI="QCI值：6-GBR优级6\\-Budget300ms"
        elif [[ "$QCI2" =~ "7" ]]; then
            QCI="QCI值：7-GBR优级7\\-Budget100ms"
        elif [[ "$QCI2" =~ "8" ]]; then
            QCI="QCI值：8-GBR优级8\\-Budget300ms"
        elif [[ "$QCI2" =~ "9" ]]; then
            QCI="QCI值：9-GBR优级9\\-延时预300ms"
        else
            QCI="锁频\锁小区\等模式下均无法查看"
        fi
    }
    Read_signal_data(){
        # Echo/blank lines and unsolicited reports do not have fixed positions.
        # Keep only the requested serving-cell record, with RAT as field one.
        OX=$(atsd_tools_cli -i cpe -c 'AT^MONSC' | tr -d '\r' \
            | sed -n 's/^[[:space:]]*\^MONSC:[[:space:]]*//p' \
            | grep -E '^(NR|LTE),' | head -n 1)
        case "$OX" in
            NR,*) DTAT_5G ;;
            LTE,*) DTAT_4G ;;
            *) return 1 ;;
        esac
    }
    Signal_percent(){
        CSQ_PER="-"
        if ! [ "$RSCP" -ge -156 ] 2>/dev/null || ! [ "$RSCP" -le -31 ] 2>/dev/null; then
            return
        fi
        if [ "$RSCP" -ge -85 ]; then
            CSQ_PER="100%"
        elif [ "$RSCP" -ge -90 ]; then
            CSQ_PER="90%"
        elif [ "$RSCP" -ge -95 ]; then
            CSQ_PER="80%"
        elif [ "$RSCP" -ge -105 ]; then
            CSQ_PER="50%"
        elif [ "$RSCP" -gt -115 ]; then
            CSQ_PER="10%"
        else
            CSQ_PER="0%"
        fi
    }
    DTAT_5G(){
        MODE="NR-5G"
        RSCP=$(echo $OX | awk -F ',' '{print $9}')
        Signal_percent
        # NR MONSC reports RSRP/RSRQ/SINR, not RSSI.
        CSQ_RSSI="-"
        ECIO=$(echo $OX | awk -F ',' '{print $10}')
        SINR=$(echo $OX | awk -F ',' '{print $11}')
        COPS_MCC=$( echo $OX | awk -F ',' '{print $2}')
        COPS_MNC=$( echo $OX | awk -F ',' '{print $3}')
        LAC=$( echo $OX | awk -F ',' '{print $8}' | sed '/^$/d')
        CID=$( echo $OX | awk -F ',' '{print $6}' | sed '/^$/d')
        CHANNEL=$( echo $OX | awk -F ',' '{print $4}')
        HFREQINFO=$(atsd_tools_cli -i cpe -c "AT^HFREQINFO?" | tr -d '\r' \
            | sed -n 's/^[[:space:]]*\^HFREQINFO:[[:space:]]*//p' \
            | awk -F ',' '$2 == 7 { print; exit }')
        band=$(echo $HFREQINFO | cut -d, -f3)   
        dl_bwN=$(echo $HFREQINFO | cut -d, -f6)
        ul_bwN=$(echo $HFREQINFO | cut -d, -f9)
        if [[ "$band" =~ ^[0-9]+$ && "$dl_bwN" =~ ^[0-9]+$ && "$ul_bwN" =~ ^[0-9]+$ ]]; then
            dl_bwN=$((dl_bwN / 1000))
            ul_bwN=$((ul_bwN / 1000))
            LBAND="N$band (Bandwidth $dl_bwN MHz Down | $ul_bwN MHz Up)"
        fi
        PCI=$( echo $OX | awk -F ',' '{print $7}' | sed '/^$/d')
        if [[ "$PCI" =~ ^[0-9A-Fa-f]+$ ]]; then PCI=$((16#$PCI)); else PCI="-"; fi
        zbjhox=$(atsd_tools_cli -i cpe -c 'AT^CASCELLINFO?' | grep 'ERROR')
        if [ "$zbjhox" == "" ] ; then
            R2cc=$(atsd_tools_cli -i cpe -c 'AT^CASCELLINFO?' | grep 'CASCELLINFO: 1')
            R3cc=$(atsd_tools_cli -i cpe -c 'AT^CASCELLINFO?' | grep 'CASCELLINFO: 2')
        else
            R2cc="Loss-in"
            R3cc="Loss-in"
        fi
    }

    DTAT_4G(){
        MODE="LTE-4G"
        CSQ_RSSI=$(echo $OX | awk -F ',' '{print $10}')
        RSCP=$(echo $OX | awk -F ',' '{print $8}')
        Signal_percent
        ECIO=$(echo $OX | awk -F ',' '{print $9}')
        SINR="\\"
        COPS_MCC=$( echo $OX | awk -F ',' '{print $2}')
        COPS_MNC=$( echo $OX | awk -F ',' '{print $3}')
        LAC=$( echo $OX | awk -F ',' '{print $7}' | sed '/^$/d')
        CID=$( echo $OX | awk -F ',' '{print $5}' | sed '/^$/d')
        CHANNEL=$( echo $OX | awk -F ',' '{print $4}')
        HFREQINFO=$(atsd_tools_cli -i cpe -c "AT^HFREQINFO?" | tr -d '\r' \
            | sed -n 's/^[[:space:]]*\^HFREQINFO:[[:space:]]*//p' \
            | awk -F ',' '$2 == 6 { print; exit }')
        band=$(echo $HFREQINFO | cut -d, -f3)   
        dl_bwN=$(echo $HFREQINFO | cut -d, -f6)
        ul_bwN=$(echo $HFREQINFO | cut -d, -f9)
        if [[ "$band" =~ ^[0-9]+$ && "$dl_bwN" =~ ^[0-9]+$ && "$ul_bwN" =~ ^[0-9]+$ ]]; then
            dl_bwN=$((dl_bwN / 1000))
            ul_bwN=$((ul_bwN / 1000))
            LBAND="B$band (Bandwidth $dl_bwN MHz Down | $ul_bwN MHz Up)"
        fi
        PCI=$( echo $OX | awk -F ',' '{print $6}' | sed '/^$/d')
        if [[ "$PCI" =~ ^[0-9A-Fa-f]+$ ]]; then PCI=$((16#$PCI)); else PCI="-"; fi
        zbjhox=$(atsd_tools_cli -i cpe -c 'AT^CASCELLINFO?' | grep 'ERROR')
        if [ "$zbjhox" == "" ] ; then
            R2cc=$(atsd_tools_cli -i cpe -c 'AT^CASCELLINFO?' | grep 'CASCELLINFO: 1')
            R3cc=$(atsd_tools_cli -i cpe -c 'AT^CASCELLINFO?' | grep 'CASCELLINFO: 2')
        else
            R2cc="Loss-in"
            R3cc="Loss-in"
        fi
    }
    AMBR(){
        # DSAMBR supports CID 1 and has no APN field. The official ODU uses
        # its own active CID; read that session's APN from the shared cache.
        apn=$(/usr/libexec/myos-nbcpe-status apn 2>/dev/null)
        DOWNspeed="-"
        UPspeed="-"
    }
    Read_module_data_AT(){
        identity=$(atsd_tools_cli -i cpe -c "ATI" | tr -d '\r')
        ModuleName=$(printf '%s\n' "$identity" | sed -n '/^Manufacturer:/p' | head -n 1)
        ModuleType=$(printf '%s\n' "$identity" | sed -n '/^Model:/p' | head -n 1)
        Moduleversion=$(printf '%s\n' "$identity" | sed -n 's/^Revision:[[:space:]]*//p' | head -n 1)
        Temperature
        ISP_Read
        TD_Tech_Ltd_SIMINFO
        Read_signal_data || return 1
        AMBR
    }

    neighbor(){
    list=$(atsd_tools_cli -i cpe -c 'AT^MONNC')
    for file in $list
    do  
        if echo "$file" | grep -q "NR" || echo "$file" | grep -q "LTE"; then
            pci_10=$((0x$(echo $file | awk -F ',' '{print $3}' | tr -d '\r\n')))
            echo $file | awk -F ',' -v pci="$pci_10" '{printf("Mode:%s earfcn:%s pci:%s RSRP:%s\n", $1, $2, pci, $4, $5, $6)}'
        fi  
    done
    }

    InitData(){
        Date=''
        CHANNEL="-" 
        ECIO="-"
        RSCP="-"
        CSQ_RSSI="-"
        CSQ_PER="-"
        ECIO1=" "
        RSCP1=" "
        NETMODE="-"
        LBAND="-"
        PCI="-"
        CTEMP="-"
        MODE="-"
        SINR="-"
        IMEI='-'
        IMSI='-'
        ICCID='-'
        phone='-'
        conntype=''
        Model=''
    }

    sim_sel=$(cat /tmp/sim_sel_AK68)
    SIMCard=""
    case $sim_sel in
        0)
            SIMCard="外置SIM卡"
            ;;
        1)
            SIMCard="内置SIM1"
            ;;
        2)
            SIMCard="内置SIM2"
            ;;
        3)
            SIMCard="外置SIM2"
            ;;
        4)
            SIMCard="外置SIM3"
            ;;
        5)
            SIMCard="外置SIM4"
            ;;
        *)
            SIMCard="SIM未读取"
            ;;
    esac
    OutData(){
        {
        echo "$ModuleName" 
        echo "$ModuleType" 
        echo "$Moduleversion" 
        echo "$CTEMP" # 
        echo `date "+%Y-%m-%d %H:%M:%S"` # 
        #----------------------------------
        echo "$SIMCard" # 卡槽
        echo "$ISP" #运营商
        echo "$IMEI" #imei
        echo "$IMSI" #imsi
        echo "$ICCID" #iccid
        echo "$phone" #phone
        #-----------------------------------
        echo "$MODE" #蜂窝网络类型 NR5G-SA "TDD"
        echo "$CSQ_PER" #CSQ_PER 信号质量
        echo "$CSQ_RSSI" #信号强度 RSSI 信号强度
        echo "$ECIO dB" #接收质量 RSRQ 
        echo "$RSCP dBm" #接收功率 RSRP
        echo "$SINR" #信噪比 SINR  rv["sinr"]
        #-----------------------------------
        echo "$COPS_MCC /$COPS_MNC" #MCC / MNC
        echo "$LAC"  #位置区编码
        echo "$CID"  #小区基站编码
        echo "$LBAND" # 频段 频宽
        echo "$CHANNEL" # 频点
        echo "$PCI" #物理小区标识   
        echo "$apn" 
        echo "$DOWNspeed"
        echo "$UPspeed"
        echo "$QCI"
        echo "$Zbjh" #载波聚合
        echo "$R2cc"
        echo "$R3cc"
        neighbor
        } > /tmp/cpe_cell-AK68.file.new
        mv /tmp/cpe_cell-AK68.file.new /tmp/cpe_cell-AK68.file
        echo "数据写出前端页面完成" > /tmp/stsss.file
        echo "数据写出完成"
    }
    #====================================================================================
    InitData
    # A busy/failed AT read must not replace a usable snapshot with dashes or
    # advance its timestamp. Readers expire the previous sample normally.
    Read_module_data_AT || exit 1
    OutData
} 200>"$LOCKFILE"
