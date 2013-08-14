#!/usr/bin/env python
from __future__ import print_function

from collections import defaultdict
from optparse import OptionParser
from os.path import join as pathjoin
import re
from sys import argv, stdin, stdout, stderr

# Reads experiment files from current directory (as dumped by do_stuff)
# and outputs results to stdout in the following CSV format:
# num_requests num_errors num_recomendations

#
# Helper functions
#
def avg(a):
	return sum(a) / len(a)

def stats(a):
	return min(a), avg(a), max(a)

#
# Process command-line
#
parser = OptionParser(usage = "usage: %prog DIRECTORIES...")
(options, args) = parser.parse_args()

totalRecommendationsForController = defaultdict(lambda: [])
totalSuccessfulRequestsForController = defaultdict(lambda: [])

for directory in args:
	#
	# Read all input files
	#
	expLogLines = open(pathjoin(directory, 'exp.log')).readlines()
	clientLogLines = open(pathjoin(directory, 'httpmon.log')).readlines()
	lcLogLines = open(pathjoin(directory, 'lc.log')).readlines()
	paramLogLines = open(pathjoin(directory, 'params')).readlines()

	#
	# Process lines
	#

	pole = '-'
	serviceLevel = '-'
	patternId = '-'
	for line in paramLogLines:
		try:
			key, value = line.strip().split('=')
			if key == 'pole':
				pole = float(value)
			elif key == 'serviceLevel':
				serviceLevel = float(value)
			elif key == 'patternId':
				patternId = int(value)
		except ValueError:
			print("Ignoring line {0}".format(line.strip()), file = stderr)

	lastTotal = 0

	totalRequests = 0
	totalErrors = 0
	totalRecommendations = 0

	for line in clientLogLines:
		try:
			rr = float(re.search("rr=([0-9.]+)", line).group(1))
			errors = int(re.search("errors=([0-9.]+)", line).group(1))
			total = int(re.search("total=([0-9.]+)", line).group(1))

			numRequests = total - lastTotal
			lastTotal = total
			numRecommendations = int(round(numRequests * rr / 100))
			numErrors = errors

			totalRequests += numRequests
			totalRecommendations += numRecommendations
			totalErrors += numErrors
		except AttributeError:
			print("Ignoring line {0}".format(line.strip()), file = stderr)
	
	totalRecommendationsForController[(pole, serviceLevel)].append(totalRecommendations)
	totalSuccessfulRequestsForController[(pole, serviceLevel)].append(totalRequests - totalErrors)

#
# Output results
#
print("% Generated using: " + ' '.join(argv))
for pole, serviceLevel in sorted(totalRecommendationsForController):
	print(pole, serviceLevel, *stats(totalRecommendationsForController[(pole, serviceLevel)]) +stats(totalSuccessfulRequestsForController[(pole, serviceLevel)]), sep = ',')
