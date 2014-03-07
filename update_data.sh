last_co2=U
last_temp=U
last_6d=U
last_56=U

update_rrd() {
    rrdtool update graph.rrd --template CO2:TEMP:x6d:x56 -- N:$last_co2:$last_temp:$last_6d:$last_56
}

while true; do
    sudo ./co2mon | grep '^ '
    sleep 1
done | tee report-$(date +%Y-%m-%d-%H-%M).log | while read -r line; do
    echo "$line"
    if [[ "$line" == *CO2:* ]]; then
        last_co2=$(echo "$line" | sed -s 's/.*CO2: //')
        update_rrd
    elif [[ "$line" == *Temperature:* ]]; then
        last_temp=$(echo "$line" | sed -s 's/.*Temperature: //')
        update_rrd
    elif [[ "$line" == *0x6d* ]]; then
        last_6d=$(echo "$line" | sed -s 's/.*: //')
        update_rrd
    elif [[ "$line" == *0x56* ]]; then
        last_56=$(echo "$line" | sed -s 's/.*: //')
        update_rrd
    fi
done
