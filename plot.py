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
capChanges = []

t0 = None
for line in stdin.readlines():
	try:
		timestamp = float(re.search("\[([0-9.]+)\]", line).group(1))
		avgLatency = float(re.search("latency=[0-9.]+:[0-9.]+:[0-9.]+:[0-9.]+:[0-9.]+:\(([0-9.]+)\)", line).group(1))
		minLatency = float(re.search("latency=([0-9.]+):[0-9.]+:[0-9.]+:[0-9.]+:[0-9.]+:\([0-9.]+\)", line).group(1))
		maxLatency = float(re.search("latency=[0-9.]+:[0-9.]+:[0-9.]+:[0-9.]+:([0-9.]+):\([0-9.]+\)", line).group(1))
		rr = float(re.search("rr=([0-9.]+)", line).group(1))
		if t0 is None:
			t0 = timestamp
		timestampSeries.append(timestamp-t0)
		avgLatencySeries.append(avgLatency)
		maxLatencySeries.append(maxLatency)
		recommenderRatioSeries.append(rr)
		processed = True
	except AttributeError:
		try:
			timestamp = float(re.match("([0-9.]+)", line).group(1))
			cap = float(re.search("cap=([0-9.]+)", line).group(1))
			if t0 is None:
				t0 = timestamp
			timestamp = float(timestamp) - float(t0)
			capChanges.append((timestamp, cap))
			processed = True
		except AttributeError:
			print("Ignoring line: " + line.strip(), file = stderr)


ax1 = plt.subplot(111)
ax1.set_xlabel('time (s)')
ax1.set_ylabel('latency (ms)')
ax1.set_yscale('log')
ax1.set_xlim(0, 7*60)
ax1.grid()

ax2 = ax1.twinx()
ax2.set_ylabel('service level (%)', color = 'g')
ax2.set_xlim(0, 7*120)

ax1.plot(timestampSeries, avgLatencySeries, 'b', label = 'avg. latency')
ax1.plot(timestampSeries, maxLatencySeries, 'r', label = 'max. latency')
ax2.plot(timestampSeries, recommenderRatioSeries, 'g', label = 'recommender ratio')
ax1.axhline(y = 1000, color = 'black', lw = 2)
for timestamp, cap in capChanges:
	ax2.text(timestamp+1, 97, str(cap))
	ax2.axvline(x = timestamp, color = 'black', lw = 2)
ax1.legend(loc = 4)
plt.show()
