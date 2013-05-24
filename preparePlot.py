#!/usr/bin/env python
from __future__ import print_function

import re
from sys import stdin, stderr

t0 = None
caps = [ 400, 200, 100, 50, 100, 200, 400 ]
fData = open('control.data', 'w')
fPlot = open('control.plot', 'w')
for line in stdin.readlines():
	try:
		timestamp = re.search("\[([0-9.]+)\]", line).group(1)
		avgLatency = re.search("latency=[0-9.]+:[0-9.]+:[0-9.]+:[0-9.]+:[0-9.]+:\(([0-9.]+)\)", line).group(1)
		minLatency = re.search("latency=([0-9.]+):[0-9.]+:[0-9.]+:[0-9.]+:[0-9.]+:\([0-9.]+\)", line).group(1)
		maxLatency = re.search("latency=[0-9.]+:[0-9.]+:[0-9.]+:[0-9.]+:([0-9.]+):\([0-9.]+\)", line).group(1)
		rr = re.search("rr=([0-9.]+)", line).group(1)
		if t0 is None:
			t0 = timestamp
		print(float(timestamp)-float(t0), minLatency, avgLatency, maxLatency, rr, file = fData)
		processed = True
	except AttributeError:
		try:
			timestamp = re.match("([0-9.]+)", line).group(1)
			if t0 is None:
				t0 = timestamp
			timestamp = float(timestamp) - float(t0)
			cap = caps.pop(0)
			print(
"""
t = {timestamp}
cap = '{cap}'
set arrow nohead from first t,graph 0 to first t, graph 1
set label cap at first t,graph 1 offset 0,-0.4
""".format(**locals()), file = fPlot)
			processed = True
		except AttributeError:
			print("Ignoring line: " + line.strip(), file = stderr)

print(
"""
fn = 'control.data'

set logscale y
set ytics nomirror
set y2tics

set xlabel "Experiment time"
set ylabel "Latency (ms)" tc lt 1
set y2label "Recommendation rate (%)" tc lt 2

set arrow nohead from graph 0,first 500 to graph 1, first 500

set title "100 clients with think time 0.1s"

plot \
	fn u 1:3 t "Latency" lt 1, \
	fn u 1:5 axis x1y2 t "Recommendation rate" lt 2
""", file = fPlot)

fData.close()
fPlot.close()
