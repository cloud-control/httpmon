#!/usr/bin/env python
from __future__ import print_function, division

from collections import defaultdict
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
	timeseries = defaultdict(lambda: {})
	for line in lines:
		try:
			timestamp = float(re.search("\[([0-9.]+)\]", line).group(1))
			avgLatency = float(re.search("latency=[0-9.]+:[0-9.]+:[0-9.]+:[0-9.]+:[0-9.]+:\(([0-9.]+)\)", line).group(1))
			minLatency = float(re.search("latency=([0-9.]+):[0-9.]+:[0-9.]+:[0-9.]+:[0-9.]+:\([0-9.]+\)", line).group(1))
			maxLatency = float(re.search("latency=[0-9.]+:[0-9.]+:[0-9.]+:[0-9.]+:([0-9.]+):\([0-9.]+\)", line).group(1))
			rr = float(re.search("rr=([0-9.]+)", line).group(1))
			throughput = float(re.search("throughput=([0-9.]+)rps", line).group(1))

			timestamp = int(timestamp)
			timeseries[timestamp]['latency'] = maxLatency / 1000.0
			timeseries[timestamp]['serviceLevel'] = rr / 100.0
			timeseries[timestamp]['throughput'] = throughput
		except AttributeError:
			try:
				m = re.search("\[([0-9.]+)\] set (.*)=(.*)", line)
				timestamp = float(m.group(1))
				key = m.group(2)
				value = float(m.group(3))

				timestamp = int(timestamp)
				timeseries[timestamp][key] = value
			except AttributeError:
				print("Ignoring line:", line.strip(), file = stderr)
	return timeseries

def getExpTimeSeries(lines):
	timeseries = defaultdict(lambda: {})
	for line in lines:
		try:
			m = re.search("\[([0-9.]+)\] (.*)=(.*)", line)
			timestamp = float(m.group(1))
			key = m.group(2)
			value = float(m.group(3))

			timestamp = int(timestamp)
			timeseries[timestamp][key] = value
		except AttributeError:
			print("Ignoring line:", line.strip(), file = stderr)
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
clientLogLines = open(inExpDir('httpmon.log')).readlines()

#
# Process lines
#

timeseries = {}
mergeIntoTimeSeries(timeseries, getClientTimeSeries(clientLogLines), "client_")
mergeIntoTimeSeries(timeseries, getExpTimeSeries(expLogLines), "exp_")

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
# Aggregate results
#
tz = defaultdict(lambda: defaultdict(lambda: []))
for timestamp, keyvalues in timeseries.iteritems():
	try:
		cap = keyvalues['exp_cap']
		concurrency = keyvalues['exp_concurrency']
		throughput = keyvalues['client_throughput']
		maxLatency = keyvalues['client_latency']

		tz[(cap, concurrency)]['throughput'].append(throughput)
		tz[(cap, concurrency)]['latency'].append(maxLatency)
	except KeyError:
		print('Missing data for timestamp {0}'.format(timestamp), file = stderr)

#
# Output results
#
if options.output is None:
	f = stdout
else:
	f = open(options.output, 'w')
print("# Generated using: " + ' '.join(argv), file = f)
for cap, concurrency in sorted(tz):
	print(cap, concurrency, avg(tz[(cap, concurrency)]['throughput']), max(tz[(cap, concurrency)]['latency']), sep = ',')
