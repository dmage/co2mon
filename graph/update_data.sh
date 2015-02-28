#!/bin/bash
last_co2=U
last_temp=U

update_rrd() {
    rrdtool update graph.rrd --template CO2:TEMP -- N:$last_co2:$last_temp
}

trap 'kill $(jobs -p)' EXIT

../build/co2mond &

PYTHONUNBUFFERED=1 ./monitor.py | while read -r name value; do
    echo "$name $value"
    if [[ "$name" == "CO2" ]]; then
        last_co2=$value
        update_rrd
    elif [[ "$name" == "TEMP" ]]; then
        last_temp=$value
        update_rrd
    fi
done
