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

## See also

  * [ZyAura ZG01C Module Manual](http://www.zyaura.com/support/manual/pdf/ZyAura_CO2_Monitor_ZG01C_Module_ApplicationNote_141120.pdf)
  * [RevSpace CO2 Meter Hacking](https://revspace.nl/CO2MeterHacking)
