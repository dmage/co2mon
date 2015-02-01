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

## D-Bus Methods

    dbus-send --session --dest=io.github.dmage.CO2Mon --type=method_call --print-reply /io/github/dmage/CO2Mon io.github.dmage.CO2Mon.GetTemperature
    dbus-send --session --dest=io.github.dmage.CO2Mon --type=method_call --print-reply /io/github/dmage/CO2Mon io.github.dmage.CO2Mon.GetCO2

## See also

  * [ZyAura ZG01C Module Manual](http://www.zyaura.com/support/manual/pdf/ZyAura_CO2_Monitor_ZG01C_Module_ApplicationNote_141120.pdf)
  * [RevSpace CO2 Meter Hacking](https://revspace.nl/CO2MeterHacking)
  * [Photos of the device and the circuit board](http://habrahabr.ru/company/masterkit/blog/248403/)
  * [Pre-alpha of GNOME Shell Extension](https://github.com/dmage/gnome-shell-extensions-co2mon)
