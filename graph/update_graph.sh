#!/bin/sh
while true; do
    make graph-all-1d.png
    for i in {1..12}; do
        make graph-all-1h.png
        sleep 5
    done
done
