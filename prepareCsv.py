#!/usr/bin/env python
from __future__ import print_function

from optparse import OptionParser
import os
import re
from sys import argv, stdin, stdout, stderr

# Reads experiment files from current directory (as dumped by do_stuff)
# and outputs results to stdout in the following CSV format:
# TIME, T_RUBIS, S_RUBIS

#
# Helper functions
#
def avg(a):
	return sum(a) / len(a)
def inExpDir(f):
	global options
	return os.path.join(options.directory, f)

#
# Process command-line
#
parser = OptionParser()
parser.add_option("-d", "--directory",
	metavar = "DIR",
	help = "look for experiment data in DIR (default: %default)",
	default = os.getcwd())
parser.add_option("-i", "--interval",
	metavar = "INTERVAL",
	help = "aggregate data over INTERVAL seconds (default: %default)",
	type = int,
	default = 1)
parser.add_option("-o", "--output",
	metavar = "OUTPUT",
	help = "output file (default: stdout)")
(options, args) = parser.parse_args()

#
# Read all input files
#
expLogLines = open(inExpDir('exp.log')).readlines()
clientLogLines = open(inExpDir('httpmon.log')).readlines()
lcLogLines = open(inExpDir('lc.log')).readlines()

#
# Process lines
#

# Extract time series
tRubis = {}
rRubis = {}
sRubis = {}

for line in clientLogLines:
	try:
		timestamp = float(re.search("\[([0-9.]+)\]", line).group(1))
		avgLatency = float(re.search("latency=[0-9.]+:[0-9.]+:[0-9.]+:[0-9.]+:[0-9.]+:\(([0-9.]+)\)", line).group(1))
		minLatency = float(re.search("latency=([0-9.]+):[0-9.]+:[0-9.]+:[0-9.]+:[0-9.]+:\([0-9.]+\)", line).group(1))
		maxLatency = float(re.search("latency=[0-9.]+:[0-9.]+:[0-9.]+:[0-9.]+:([0-9.]+):\([0-9.]+\)", line).group(1))
		rr = float(re.search("rr=([0-9.]+)", line).group(1))
		tRubis[int(timestamp)] = maxLatency
		rRubis[int(timestamp)] = rr
	except AttributeError:
		pass

for line in lcLogLines:
	try:
		timestamp = float(re.search("([0-9.]+)", line).group(1))
		rr = float(re.search("rr=([0-9.]+)", line).group(1))
		sRubis[int(timestamp)] = rr
	except AttributeError:
		pass

# Get experiment start
for line in expLogLines:
	try:
		timestamp = float(re.search("\[([0-9.]+)\] start", line).group(1))
		tStart = int(timestamp)
		break
	except AttributeError:
		pass

# Get experiment end
tEnd = min(max(tRubis), max(sRubis))

#
# Output results
#
if options.output is None:
	f = stdout
else:
	f = open(options.output, 'w')
print("% Generated using: " + ' '.join(argv), file = f)

aggregateInterval = options.interval
for time in range(tStart, tEnd, aggregateInterval):
	latencies = []
	recommandationRates = []
	serviceLevels = []
	for t in range(time, time + aggregateInterval):
		try:
			latencies.append(tRubis[t])
			recommandationRates.append(rRubis[t])
			serviceLevels.append(sRubis[t])
		except KeyError:
			print("Missing data for absolute time {0}, relative time {1}".format(t, t-tStart), file = stderr)
			continue
	if len(latencies) == 0: continue
	if len(serviceLevels) == 0: continue
	print(time-tStart, max(latencies) / 1000, avg(recommandationRates) / 100, avg(serviceLevels) / 100, sep = ',', file = f)
