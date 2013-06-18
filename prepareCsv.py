#!/usr/bin/env python
from __future__ import print_function

from optparse import OptionParser
import os
import re
from sys import stdin, stderr

# Reads experiment files from current directory (as dumped by do_stuff)
# and outputs results to stdout in the following CSV format:
# TIME, T_RUBIS, S_RUBIS

#
# Helper functions
#
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
for time in range(tStart, tEnd):
	try:
		print(time-tStart, tRubis[time] / 1000, sRubis[time] / 100, sep = ',')
	except KeyError:
		print("Missing data for absolute time {0}, relative time {1}".format(time, time-tStart), file = stderr)
