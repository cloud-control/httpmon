#!/usr/bin/env python
from __future__ import print_function, division

from optparse import OptionParser
import os
import re
from sys import argv, stdin, stdout, stderr

# Reads experiment files from current directory (as dumped by do_stuff)
# and outputs results to stdout in the following CSV format:
# TIME, T_RUBIS, S_RUBIS

# timeseries: a map from an integer time instant expressed in seconds, to a key-value map

#
# Helper functions
#
def avg(a):
	if len(a) == 0:
		return float('nan')
	return sum(a) / len(a)
def inExpDir(f):
	global options
	return os.path.join(options.directory, f)
def saturate(v, minValue = -1, maxValue = +1):
	return min(max(v, minValue), maxValue)

def getAggregateValue(series, key, start, end, func):
	values = []
	for timestamp, keyvalues in series.iteritems():
		assert(int(timestamp) == timestamp)
		if timestamp < start: continue
		if timestamp >= end: continue
		values.append(keyvalues[key])
	if len(values) == 0:
		return float('nan')
	return func(values)

def mergeIntoTimeSeries(mainSeries, series, prefix = '', suffix = ''):
	for timestamp, keyvalues in series.iteritems():
		assert(int(timestamp) == timestamp)
		if timestamp not in mainSeries:
			mainSeries[timestamp] = {}
		for key, value in keyvalues.iteritems():
			mainSeries[timestamp][prefix + key + suffix] = value
	return mainSeries # for chaining

def addEchoToTimeSeries(series):
	lastKeyValues = {}
	for timestamp in sorted(series.iterkeys()):
		assert(int(timestamp) == timestamp)
		keyvalues = timeseries[timestamp]
		for key, value in lastKeyValues.iteritems():
			if key not in keyvalues:
				series[timestamp][key] = lastKeyValues[key]
		lastKeyValues = dict(series[timestamp])
	return series # for chaining

def getClientTimeSeries(lines):
	timeseries = {}
	for line in lines:
		try:
			timestamp = float(re.search("\[([0-9.]+)\]", line).group(1))
			avgLatency = float(re.search("latency=[0-9.]+:[0-9.]+:[0-9.]+:[0-9.]+:[0-9.]+:\(([0-9.]+)\)", line).group(1))
			minLatency = float(re.search("latency=([0-9.]+):[0-9.]+:[0-9.]+:[0-9.]+:[0-9.]+:\([0-9.]+\)", line).group(1))
			maxLatency = float(re.search("latency=[0-9.]+:[0-9.]+:[0-9.]+:[0-9.]+:([0-9.]+):\([0-9.]+\)", line).group(1))
			rr = float(re.search("rr=([0-9.]+)", line).group(1))

			timestamp = int(timestamp)
			if timestamp not in timeseries:
				timeseries[timestamp] = {}
			timeseries[timestamp]['latency'] = maxLatency / 1000.0
			timeseries[timestamp]['serviceLevel'] = rr / 100.0
		except AttributeError:
			print("Ignoring line:", line.strip(), file = stderr)
			pass
	return timeseries

def getLcTimeSeries(lines):
	timeseries = {}
	for line in lines:
		try:
			timestamp = float(re.search("([0-9.]+)", line).group(1))
			rr = float(re.search("rr=([0-9.]+)", line).group(1))
			perf = float(re.search("perf=([0-9.-]+)", line).group(1))

			timestamp = int(timestamp)
			if timestamp not in timeseries:
				timeseries[timestamp] = {}
			timeseries[timestamp]['serviceLevel'] = rr / 100.0
			timeseries[timestamp]['perf'] = perf
		except AttributeError:
			pass
	return timeseries

def getRmTimeSeries(lines):
	timeseries = {}
	for line in lines:
		try:
			timestamp = float(re.search("([0-9.]+)", line).group(1))
			vm = re.search("vm=([a-zA-Z0-9.-]+)", line).group(1)
			cap = float(re.search("cap=([0-9.]+)", line).group(1))

			timestamp = int(timestamp)
			if timestamp not in timeseries:
				timeseries[timestamp] = {}
			timeseries[timestamp][vm] = cap
		except AttributeError:
			pass
	return timeseries

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
# Read input files
#
expLogLines = open(inExpDir('exp.log')).readlines()
client1LogLines = open(inExpDir('httpmon1.log')).readlines()
client2LogLines = open(inExpDir('httpmon2.log')).readlines()
client3LogLines = open(inExpDir('httpmon3.log')).readlines()
client4LogLines = open(inExpDir('httpmon4.log')).readlines()
lc1LogLines = open(inExpDir('lc1.log')).readlines()
lc2LogLines = open(inExpDir('lc2.log')).readlines()
lc3LogLines = open(inExpDir('lc3.log')).readlines()
lc4LogLines = open(inExpDir('lc4.log')).readlines()
rmLogLines = open(inExpDir('rm.log')).readlines()

#
# Process lines
#

timeseries = {}
mergeIntoTimeSeries(timeseries, getLcTimeSeries(lc1LogLines), "vm1_")
mergeIntoTimeSeries(timeseries, getLcTimeSeries(lc2LogLines), "vm2_")
mergeIntoTimeSeries(timeseries, getLcTimeSeries(lc3LogLines), "vm3_")
mergeIntoTimeSeries(timeseries, getLcTimeSeries(lc4LogLines), "vm4_")

mergeIntoTimeSeries(timeseries, getClientTimeSeries(client1LogLines), "client1_")
mergeIntoTimeSeries(timeseries, getClientTimeSeries(client2LogLines), "client2_")
mergeIntoTimeSeries(timeseries, getClientTimeSeries(client3LogLines), "client3_")
mergeIntoTimeSeries(timeseries, getClientTimeSeries(client4LogLines), "client4_")

mergeIntoTimeSeries(timeseries, getRmTimeSeries(rmLogLines), "rm_")

addEchoToTimeSeries(timeseries)

# Get experiment start
for line in expLogLines:
	try:
		timestamp = float(re.search("\[([0-9.]+)\] start", line).group(1))
		tStart = int(timestamp)
		break
	except AttributeError:
		pass
tEnd = max(timeseries.iterkeys())

#
# Output results
#
if options.output is None:
	f = stdout
else:
	f = open(options.output, 'w')
print("# Generated using: " + ' '.join(argv), file = f)

print("time cap1 cap2 cap3 cap4 perf1 perf2 perf3 perf4 sl1 sl2 sl3 sl4 latency1 latency2 latency3 latency4",
	sep = ',', file = f)
for t in range(tStart, tEnd, options.interval):
	try:
		print( \
			t - tStart,
			getAggregateValue(timeseries, "rm_rubis-hvm", t, t+options.interval, avg),
			getAggregateValue(timeseries, "rm_rubbos-hvm", t, t+options.interval, avg),
			getAggregateValue(timeseries, "rm_rubis-hvm2", t, t+options.interval, avg),
			getAggregateValue(timeseries, "rm_rubbos-hvm2", t, t+options.interval, avg),
			saturate(getAggregateValue(timeseries, "vm1_perf", t, t+options.interval, min)),
			saturate(getAggregateValue(timeseries, "vm2_perf", t, t+options.interval, min)),
			saturate(getAggregateValue(timeseries, "vm3_perf", t, t+options.interval, min)),
			saturate(getAggregateValue(timeseries, "vm4_perf", t, t+options.interval, min)),
			getAggregateValue(timeseries, "vm1_serviceLevel", t, t+options.interval, avg),
			getAggregateValue(timeseries, "vm2_serviceLevel", t, t+options.interval, avg),
			getAggregateValue(timeseries, "vm3_serviceLevel", t, t+options.interval, avg),
			getAggregateValue(timeseries, "vm4_serviceLevel", t, t+options.interval, avg),
			getAggregateValue(timeseries, "client1_latency", t, t+options.interval, max),
			getAggregateValue(timeseries, "client2_latency", t, t+options.interval, max),
			getAggregateValue(timeseries, "client3_latency", t, t+options.interval, max),
			getAggregateValue(timeseries, "client4_latency", t, t+options.interval, max),
			sep = ',', file = f
		)
	except KeyError:
		print("Missing data for time {0}".format(t), file = stderr)
