#!/usr/bin/env python
from __future__ import print_function

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
	return sum(a) / len(a)
def inExpDir(f):
	global options
	return os.path.join(options.directory, f)

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
			timeseries[timestamp]['latency'] = maxLatency
			timeseries[timestamp]['serviceLevel'] = rr
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
			timeseries[timestamp]['serviceLevel'] = rr
			timeseries[timestamp]['perf'] = perf
		except AttributeError:
			pass
	return timeseries

def getRmTimeSeries(lines):
	timeseries = {}
	for line in lines:
		try:
			timestamp = float(re.search("([0-9.]+)", line).group(1))
			vm = re.search("vm=([a-zA-Z0-9.]+)", line).group(1)
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
lc1LogLines = open(inExpDir('lc1.log')).readlines()
lc2LogLines = open(inExpDir('lc2.log')).readlines()
rmLogLines = open(inExpDir('rm.log')).readlines()

#
# Process lines
#

timeseries = {}
mergeIntoTimeSeries(timeseries, getLcTimeSeries(lc1LogLines), "rubis_")
mergeIntoTimeSeries(timeseries, getLcTimeSeries(lc2LogLines), "rubbos_")

mergeIntoTimeSeries(timeseries, getClientTimeSeries(client1LogLines), "rubis_")
mergeIntoTimeSeries(timeseries, getClientTimeSeries(client2LogLines), "rubbos_")

mergeIntoTimeSeries(timeseries, getRmTimeSeries(rmLogLines))

addEchoToTimeSeries(timeseries)

# Get experiment start
for line in expLogLines:
	try:
		timestamp = float(re.search("\[([0-9.]+)\] start", line).group(1))
		tStart = int(timestamp)
		break
	except AttributeError:
		pass
tEnd = tStart + 600 #max(timeseries.iterkeys())

#
# Output results
#
if options.output is None:
	f = stdout
else:
	f = open(options.output, 'w')
print("# Generated using: " + ' '.join(argv), file = f)

for t in range(tStart, tEnd):
	try:
		print( \
			t - tStart,
			timeseries[t].get("rubis", '-'),
			timeseries[t].get("rubbos", '-'),
			timeseries[t].get("rubis_perf", '-'),
			timeseries[t].get("rubbos_perf", '-'),
			timeseries[t].get("rubis_serviceLevel", '-'),
			timeseries[t].get("rubbos_serviceLevel", '-'),
			sep = ',', file = f
		)
	except KeyError:
		print("Missing data for time {0}".format(t), file = stderr)
