#!/bin/bash

# Settings
vm=rubis-hvm
pole=$1
serviceLevel=$2
patternId=$3
httpmon=./httpmon
actuator=./actuator
lc=./rubis/PHP/localController.py
url="http://rubis-hvm/PHP/RandomItem.php"

case $patternId in
	1) pattern="100 150 150 100 150 100 050 050 150 100" ;;
	2) pattern="200 250 250 200 250 200 150 150 250 200" ;;
	3) pattern="400 450 450 400 450 400 350 350 450 400" ;;
	4) pattern="600 650 650 600 650 600 550 550 650 600" ;;
	5) pattern="800 50 800 50 800 50 800 50 800 50" ;;
	6) pattern="50 800 50 800 50 800 50 800 50 800" ;;
	7) pattern="400 800 50 400 200 600 200 400 800 400" ;;
	8) pattern="250 75 125 85 650 575 275 750 75 375" ;;
	9) pattern="325 625 650 200 425 385 525 575 600 250" ;;
	0) pattern="675 475 450 775 750 600 500 325 475 100" ;;
	C) pattern="400" ;;
	*) echo "Unknown pattern Id $patternId" >&2; exit 1;;
esac

# Helper functions
function setCap {
	echo [`date +%s`] cap=$1 >&8
	$actuator --vm $vm --cap $1 &>> actuator.log
}
function setThinkTime {
	echo [`date +%s`] thinktime=$1 >&8
	echo "thinktime=$1" >&9
}
function setConcurrency {
	echo [`date +%s`] concurrency=$1 >&8
	echo "concurrency=$1" >&9
}
function setStart {
	echo [`date +%s`] start >&8
}

#
# Experimental protocol
#

# Stop on any error
set -e

# Resolve relative paths
actuator=`readlink -f $actuator`
httpmon=`readlink -f $httpmon`

# Create results directory
resultsbase=`date +%Y-%m-%dT%H:%M:%S%z`
resultsdir=exp_$resultsbase
mkdir $resultsdir
cd $resultsdir

# make sure environment is clean
pkill -f httpmon || true
ssh rubis-hvm "pkill -f localController.py" || true

# start local controller
ssh rubis-hvm "$lc --pole $pole --serviceLevel $serviceLevel" &> lc.log &
lcPid=$!

# start (but do not activate) http client
mkfifo httpmon.fifo
$httpmon --url $url --concurrency 0 --timeout 10 < httpmon.fifo &> httpmon.log &
httpmonPid=$!
exec 9> httpmon.fifo

# open log for experiment
mkfifo exp.fifo
tee exp.log < exp.fifo &
expLogPid=$!
exec 8> exp.fifo

# output starting parameters
( set -o posix ; set ) > params # XXX: perhaps too exhaustive
git log -1 > version

# change parameters
setCap 800
sleep 10 # let system settle
setStart
setThinkTime 3
setConcurrency 100
for cap in $pattern; do
	setCap $cap
	sleep 100
done
setCap 800

# stop experiment log channel
kill $expLogPid
wait $expLogPid || true

# stop http client
kill $httpmonPid
wait $httpmonPid || true

# stop local controller and copy results
kill $lcPid
wait $lcPid || true

# done
cd ..
poleId=`echo $pole | tr -d .`
serviceLevelId=`echo $serviceLevel | tr -d .`
./prepareCsv.py -d $resultsdir -o exp_${patternId}_${poleId}_${serviceLevelId}.csv

