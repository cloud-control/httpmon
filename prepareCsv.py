#!/usr/bin/env python
from __future__ import print_function

import re
from sys import stdin, stderr

# Reads experiment files from current directory (as dumped by do_stuff)
# and outputs results to stdout in the following CSV format:
# TIME, T_RUBIS, S_RUBIS

#
# Read all input files
#
expLogLines = open('exp.log').readlines()
clientLogLines = open('httpmon.log').readlines()
lcLogLines = open('lc.log').readlines()

#
# Process lines
#

# Extract time series
tRubis = {}
sRubis = {}

for line in clientLogLines:
	try:
		timestamp = float(re.search("\[([0-9.]+)\]", line).group(1))
		avgLatency = float(re.search("latency=[0-9.]+:[0-9.]+:[0-9.]+:[0-9.]+:[0-9.]+:\(([0-9.]+)\)", line).group(1))
		minLatency = float(re.search("latency=([0-9.]+):[0-9.]+:[0-9.]+:[0-9.]+:[0-9.]+:\([0-9.]+\)", line).group(1))
		maxLatency = float(re.search("latency=[0-9.]+:[0-9.]+:[0-9.]+:[0-9.]+:([0-9.]+):\([0-9.]+\)", line).group(1))
		rr = float(re.search("rr=([0-9.]+)", line).group(1))
		tRubis[int(timestamp)] = maxLatency
	except AttributeError:
		print(line)
		raise
		pass

for line in lcLogLines:
	try:
		timestamp = float(re.search("([0-9.]+)", line).group(1))
		rr = float(re.search("rr=([0-9.]+)", line).group(1))
		sRubis[int(timestamp)] = rr
	except AttributeError:
		print(line)
		raise
		pass

# Get experiment start
for line in expLogLines:
	try:
		timestamp = float(re.search("\[([0-9.]+)\]", line).group(1))
		tStart = int(timestamp)
		break
	except AttributeError:
		print(line)
		raise
		pass

# Get experiment end
tEnd = min(max(tRubis), max(sRubis))

#
# Output results
#
for time in range(tStart, tEnd):
	try:
		print(time-tStart, tRubis[time] / 1000, sRubis[time] / 100, sep = ',')
	except KeyError:
		print("Missing data for absolute time {0}, relative time {1}".format(time, time-tStart), file = stderr)
