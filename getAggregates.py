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
parser = OptionParser()
(options, args) = parser.parse_args()

totalRecommendationsForPole = defaultdict(lambda: [])
totalNonRecommendationsForPole = defaultdict(lambda: [])
totalErrorsForPole = defaultdict(lambda: [])

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
			errors = float(re.search("errors=([0-9.]+)", line).group(1))
			total = int(re.search("total=([0-9.]+)", line).group(1))

			numRequests = total - lastTotal
			numRecommendations = int(round(numRequests * rr / 100))
			numErrors = int(round(numRequests * errors / 100))

			totalRequests += numRequests
			totalRecommendations += numRecommendations
			totalErrors += numErrors
		except AttributeError:
			print("Ignoring line {0}".format(line.strip()), file = stderr)
	
	totalRecommendationsForPole[pole].append(float(totalRecommendations) / totalRequests)
	#totalNonRecommendationsForPole[pole].append(totalRequests - totalRecommendations)
	totalErrorsForPole[pole].append(float(totalErrors) / totalRequests)

#
# Output results
#
for pole in sorted(totalRecommendationsForPole):
	print(pole, *stats(totalRecommendationsForPole[pole]) +stats(totalErrorsForPole[pole]), sep = ',')
