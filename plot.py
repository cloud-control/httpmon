#!/usr/bin/env python
from __future__ import print_function

import matplotlib.pyplot as plt
import numpy as np
import re
from sys import stdin, stderr

timestampSeries = []
avgLatencySeries = []
maxLatencySeries = []
recommenderRatioSeries = []
commenterRatioSeries = []
capChanges = []

t0 = 0
for line in stdin.readlines():
	try:
		timestamp = float(re.search("\[([0-9.]+)\]", line).group(1))
		avgLatency = float(re.search("latency=[0-9.]+:[0-9.]+:[0-9.]+:[0-9.]+:[0-9.]+:\(([0-9.]+)\)", line).group(1))
		minLatency = float(re.search("latency=([0-9.]+):[0-9.]+:[0-9.]+:[0-9.]+:[0-9.]+:\([0-9.]+\)", line).group(1))
		maxLatency = float(re.search("latency=[0-9.]+:[0-9.]+:[0-9.]+:[0-9.]+:([0-9.]+):\([0-9.]+\)", line).group(1))
		rr = float(re.search("rr=([0-9.]+)", line).group(1))
		try:
			cr = float(re.search("cr=([0-9.]+)", line).group(1))
		except:
			cr = 0
		timestampSeries.append(timestamp)
		avgLatencySeries.append(avgLatency)
		maxLatencySeries.append(maxLatency)
		recommenderRatioSeries.append(rr)
		commenterRatioSeries.append(cr)
		processed = True
	except AttributeError:
		try:
			timestamp = float(re.match("([0-9.]+)", line).group(1))
			cap = float(re.search("cap=([0-9.]+)", line).group(1))
			if t0 == 0:
				t0 = timestamp
			timestamp = float(timestamp)
			capChanges.append((timestamp, cap))
			processed = True
		except AttributeError:
			print("Ignoring line: " + line.strip(), file = stderr)

# Make timestamps relative to experiment start
capChanges = [ (t-t0, c) for t,c in capChanges ]
timestampSeries = [ t-t0 for t in timestampSeries ]

ax1 = plt.subplot(111)
ax1.set_xlabel('time (s)')
ax1.set_ylabel('latency (ms)')
#ax1.set_yscale('log')
ax1.set_xlim(0, 7*60)
ax1.grid()

ax2 = ax1.twinx()
ax2.set_ylabel('recommendations / comments (%)')
ax2.set_xlim(0, max(capChanges)[0] + 120)

ax1.plot(timestampSeries, avgLatencySeries, 'b', label = 'avg. latency')
ax1.plot(timestampSeries, maxLatencySeries, 'r', label = 'max. latency')
ax1.plot([], [], 'g', label = 'recommendations (%)')
ax1.plot([], [], 'black', label = 'comments (%)')
ax2.plot(timestampSeries, recommenderRatioSeries, 'g', label = 'recommender ratio')
ax2.plot(timestampSeries, commenterRatioSeries, 'black', label = 'commenter ratio')
ax1.axhline(y = 1000, color = 'black', lw = 2)
for timestamp, cap in capChanges:
	ax2.text(timestamp+1, 97, str(cap))
	ax2.axvline(x = timestamp, color = 'black', lw = 2)
ax1.legend(loc = 5)
plt.show()
