import collectd
import os

CO2MOND_DATADIR='/var/lib/co2mon/'


def read_metric(name):
    v = open(os.path.join(CO2MOND_DATADIR, name)).read()
    try:
        return int(v)
    except ValueError:
        return float(v)


def read_callback(data=None):
    vl = collectd.Values(plugin='co2mon')
    vl.plugin = 'co2mon'
    try:
        vl.time = read_metric('heartbeat')
    except:
        pass
    co2 = [read_metric('CntR')]
    temp = [read_metric('Tamb')]
    vl.dispatch(values=co2, type='gauge', type_instance='co2_ppm')
    vl.dispatch(values=temp, type='temperature')


def configure_callback(conf):
    global CO2MOND_DATADIR
    for c in conf.children:
        if c.key == 'datadir':
            CO2MOND_DATADIR = c.values[0]


collectd.register_read(read_callback)
collectd.register_config(configure_callback)
