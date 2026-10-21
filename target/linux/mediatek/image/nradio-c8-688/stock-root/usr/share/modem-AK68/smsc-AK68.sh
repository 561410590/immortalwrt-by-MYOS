#!/bin/sh
rec=$(atsd_tools_cli -i cpe -c "at+cmgl=4")
index=0
echo "$rec" | while IFS= read -r line; do
    echo "$line" | grep -q '+CMGL:'
    if [ $? -eq 0 ]; then
        index=$(echo "$line" | awk -F '[ ,]' '{print $2}')
        length=$(echo "$line" | awk -F '[ ,]' '{print $5}')
        #read -r pdu_line_1 || break
        read -r pdu_line_1 || break
        pdu=$(echo "$pdu_line_1")
        echo "第${index}条短信" >> /tmp/smsc2-AK68.at
        echo " " >> /tmp/smsc2-AK68.at
        echo "PDU数据：" >> /tmp/smsc-AK68.at
        echo "${pdu}" >> /tmp/smsc-AK68.at
        echo "PDU解析后的内容：" >> /tmp/smsc-AK68.at
        pdurb=$(echo "${pdu}" | pdu_decoder-AK68)
        echo "${pdurb}" >> /tmp/smsc2-AK68.at
        echo " " >> /tmp/smsc2-AK68.at
        echo "------------------------------------------------------" >> /tmp/smsc2-AK68.at
        sed -e '/^Textlen=/d' -e 's/^From:/发件人:/' -e 's/^Date\/Time:/发件时间:/' /tmp/smsc2-AK68.at > /tmp/smsc-AK68.at
    fi
done

cat  /tmp/smsc-AK68.at
rm -f /tmp/smsc-AK68.at
rm -f /tmp/smsc2-AK68.at