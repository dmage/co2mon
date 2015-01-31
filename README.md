# Software for CO2 Monitor

## Usage

    mkdir build
    cd build
    cmake ..
    make

    cd ../graph/
    make graph.rrd
    ./update_graph.sh >/dev/null &
    xdg-open index.html &
    ./update_data.sh

## D-Bus Monitor

    dbus-monitor "type='signal',interface='io.github.dmage.CO2Mon',member='NewValue'"
