neighbor() {
    list=$(atsd_tools_cli -i cpe -c 'AT^MONNC')
    {
        for file in $list
        do  
            if echo "$file" | grep -q "NR" || echo "$file" | grep -q "LTE"; then
                pci_10=$((0x$(echo $file | awk -F ',' '{print $3}' | tr -d '\r\n')))
                echo $file | awk -F ',' -v pci="$pci_10" '{printf("模式%s 频点:%s 小区:%s 信号:%s\n", $1, $2, pci, $4, $5, $6)}'
            fi  
        done
    } > /tmp/LQXH-AK68.file
}
neighbor
