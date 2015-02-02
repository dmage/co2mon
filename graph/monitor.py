#!/usr/bin/env python
import dbus
import gobject
from dbus.mainloop.glib import DBusGMainLoop


def message_handler(bus, msg):
    if msg.get_interface() == "io.github.dmage.CO2Mon":
        code, raw_value, name, value = msg.get_args_list()
        if name != "UNKNOWN":
            print("{} {}".format(name, value))


def main():
    DBusGMainLoop(set_as_default=True)
    bus = dbus.SystemBus()

    string = "type='signal',interface='io.github.dmage.CO2Mon',member='NewValue'"
    bus.add_match_string(string)
    bus.add_message_filter(message_handler)

    mainloop = gobject.MainLoop()
    mainloop.run()


if __name__ == '__main__':
    main()
