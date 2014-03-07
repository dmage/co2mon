all: co2mon

co2mon: main.c
	$(CC) -g `pkg-config libusb-1.0 --cflags` `pkg-config libusb-1.0 --libs` $< -o $@

graph.rrd:
	# 17280 = 24*60*60/5
	# 12 = 60/5
	# 43200 = 30*24*60*60/60
	rrdtool create $@ --step 5 \
		\
		DS:CO2:GAUGE:60:0:3000 \
		DS:TEMP:GAUGE:60:0:40 \
		DS:x6d:GAUGE:60:0:3000 \
		DS:x56:GAUGE:60:8000:10000 \
		\
		RRA:AVERAGE:0:1:17280 \
		RRA:AVERAGE:0.5:12:43200

graph-all-1h.png: graph.rrd
	rrdtool graph graph-co2-1h.png --start end-1h --end now --width 800 --height 400 --alt-autoscale --alt-y-grid \
		DEF:co2=$^:CO2:AVERAGE \
		'LINE:co2#0000ff:co2 line'
	rrdtool graph graph-temp-1h.png --start end-1h --end now --width 800 --height 400 --alt-autoscale --alt-y-grid \
		DEF:temp=$^:TEMP:AVERAGE \
		'LINE:temp#00ff00:temp line'
	rrdtool graph graph-6d-1h.png --start end-1h --end now --width 800 --height 400 --alt-autoscale --alt-y-grid \
		DEF:x6d=$^:x6d:AVERAGE \
		'LINE:x6d#ff0000:x6d line'
	rrdtool graph graph-56-1h.png --start end-1h --end now --width 800 --height 400 --alt-autoscale --alt-y-grid \
		DEF:x56=$^:x56:AVERAGE \
		'LINE:x56#00ffff:x56 line'

graph-all-1d.png: graph.rrd
	rrdtool graph graph-co2-1d.png --start end-1d --end now --width 1200 --height 400 --alt-autoscale --alt-y-grid \
		DEF:co2=$^:CO2:AVERAGE \
		'LINE:co2#0000ff:co2 line'
	rrdtool graph graph-temp-1d.png --start end-1d --end now --width 1200 --height 400 --alt-autoscale --alt-y-grid \
		DEF:temp=$^:TEMP:AVERAGE \
		'LINE:temp#00ff00:temp line'
	rrdtool graph graph-6d-1d.png --start end-1d --end now --width 1200 --height 400 --alt-autoscale --alt-y-grid \
		DEF:x6d=$^:x6d:AVERAGE \
		'LINE:x6d#ff0000:x6d line'
	rrdtool graph graph-56-1d.png --start end-1d --end now --width 1200 --height 400 --alt-autoscale --alt-y-grid \
		DEF:x56=$^:x56:AVERAGE \
		'LINE:x56#00ffff:x56 line'
