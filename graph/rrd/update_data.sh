#!/bin/sh
last_co2=U
last_temp=U

update_rrd() {
    rrdtool update graph.rrd --template CO2:TEMP -- N:$last_co2:$last_temp
}

../../build/co2mond/co2mond | while read -r name value; do
    echo "$name $value"
    if [ "$name" = "CntR" ]; then
        last_co2=$value
        update_rrd
    elif [ "$name" = "Tamb" ]; then
        last_temp=$value
        update_rrd
    fi
done
