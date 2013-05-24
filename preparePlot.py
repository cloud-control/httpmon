#!/usr/bin/env python
from __future__ import print_function

import re
from sys import stdin, stderr

for line in stdin.readlines():
	try:
		timestamp = re.search("\[([0-9.]+)\]", line).group(1)
		avgLatency = re.search("latency=[0-9.]+:[0-9.]+:[0-9.]+:[0-9.]+:[0-9.]+:\(([0-9.]+)\)", line).group(1)
		minLatency = re.search("latency=([0-9.]+):[0-9.]+:[0-9.]+:[0-9.]+:[0-9.]+:\([0-9.]+\)", line).group(1)
		maxLatency = re.search("latency=[0-9.]+:[0-9.]+:[0-9.]+:[0-9.]+:([0-9.]+):\([0-9.]+\)", line).group(1)
		rr = re.search("rr=([0-9.]+)", line).group(1)
		print(timestamp, minLatency, avgLatency, maxLatency, rr)
	except:
		print("Ignoring line: " + line, file = stderr)
