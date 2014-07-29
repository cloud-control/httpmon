#!/bin/bash

# httpmon accepts changing some of the command-line parameters at run-time,
# by accepting commands through stdin. This is an example script that automates
# dynamic workload generation using httpmon. httpmon's output is redirected to
# httpmon.log.

#
# Experiment parameters
#

# URL to generate load for
url="http://example.com"

#
# Experiment verbs (see below for usage)
#
function setStart {
	echo [`date +%s`] start
}
function setCount {
	echo [`date +%s`] count=$1
	echo "count=$1" >&9
}
function setOpen {
	echo [`date +%s`] open=$1
	echo "open=$1" >&9
}
function setThinkTime {
	echo [`date +%s`] thinktime=$1
	echo "thinktime=$1" >&9
}
function setConcurrency {
	echo [`date +%s`] concurrency=$1
	echo "concurrency=$1" >&9
}
function setTimeout {
	echo [`date +%s`] timeout=$1
	echo "timeout=$1" >&9
}

#
# Initialization
#

# Create FIFO to communicate with httpmon and start httpmon
rm -f httpmon.fifo
mkfifo httpmon.fifo
./httpmon --url $url --concurrency 0 --timeout 30 --deterministic < httpmon.fifo &> httpmon.log &
exec 9> httpmon.fifo

#
# Initialize experiment
#
setOpen 1
setThinkTime 1
setTimeout 4
setCount 120900
setStart

#
# Run experiment
#
setConcurrency 326
sleep 100
setConcurrency 15
sleep 100
setConcurrency 382
sleep 100
setConcurrency 473
sleep 100
setConcurrency 13
sleep 100
sleep 10
