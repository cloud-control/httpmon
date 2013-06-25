#!/bin/bash

# Settings
httpmon=httpmon
resourceManager=rm
vm1=rubis-hvm
vm2=rubbos-hvm
vm3=rubis-hvm2
vm4=rubbos-hvm2
lc1=rubis/PHP/localController.py
lc2="rubbos/PHP/localController.py --setPoint 0.5"
lc3=rubis/PHP/localController.py
lc4="rubbos/PHP/localController.py --setPoint 0.5"
url1=http://rubis-hvm/PHP/RandomItem.php
url2=http://rubbos-hvm/PHP/RandomStory.php
url3=http://rubis-hvm2/PHP/RandomItem.php
url4=http://rubbos-hvm2/PHP/RandomStory.php

# Helper functions
function setThinkTime {
	echo [`date +%s`] thinktime=$2 >&9
	echo "thinktime=$2" >&$1
}
function setConcurrency {
	echo [`date +%s`] concurrency=$2 >&9
	echo "concurrency=$2" >&$1
}
function setStart {
	echo [`date +%s`] start >&9
}

#
# Experimental protocol
#

# Stop on any error
set -e

# Resolve relative paths
httpmon=`readlink -f $httpmon`
resourceManager=`readlink -f $resourceManager`

# Create results directory
resultsbase=`date +%Y-%m-%dT%H:%M:%S%z`
resultsdir=exp_$resultsbase
mkdir $resultsdir
cd $resultsdir

# make sure environment is clean
pkill -f httpmon || true
pkill -f rm || true
ssh $vm1 "pkill -f localController.py" || true
ssh $vm2 "pkill -f localController.py" || true
ssh $vm3 "pkill -f localController.py" || true
ssh $vm4 "pkill -f localController.py" || true

# start local controllers
ssh $vm1 $lc1 &> lc1.log &
pids="$pids $!"
ssh $vm2 $lc2 &> lc2.log &
pids="$pids $!"
ssh $vm3 $lc3 &> lc3.log &
pids="$pids $!"
ssh $vm4 $lc4 &> lc4.log &
pids="$pids $!"

# start (but do not activate) http client
mkfifo httpmon1.fifo
$httpmon --url $url1 --concurrency 0 --timeout 10 < httpmon1.fifo &> httpmon1.log &
pids="$pids $!"
exec 11> httpmon1.fifo
mkfifo httpmon2.fifo
$httpmon --url $url2 --concurrency 0 --timeout 10 < httpmon2.fifo &> httpmon2.log &
pids="$pids $!"
exec 12> httpmon2.fifo
mkfifo httpmon3.fifo
$httpmon --url $url3 --concurrency 0 --timeout 10 < httpmon3.fifo &> httpmon3.log &
pids="$pids $!"
exec 13> httpmon3.fifo
mkfifo httpmon4.fifo
$httpmon --url $url4 --concurrency 0 --timeout 10 < httpmon4.fifo &> httpmon4.log &
pids="$pids $!"
exec 14> httpmon4.fifo

# start resource manager
$resourceManager --epsilon 0.2 --ncpus 8 &> rm.log &
pids="$pids $!"

# open log for experiment
mkfifo exp.fifo
tee exp.log < exp.fifo &
pids="$pids $!"
exec 9> exp.fifo

# output starting parameters
( set -o posix ; set ) > params # XXX: perhaps too exhaustive
git log -1 > version

# change parameters
sleep 10 # let system settle
setStart
setThinkTime 11 3
setThinkTime 12 0.5
setThinkTime 13 3
setThinkTime 14 0.5
# t=0
setConcurrency 11 50
setConcurrency 12 10
setConcurrency 13 10
setConcurrency 14 10
sleep 200
# t=200
setConcurrency 12 100
sleep 200
# t=400
setConcurrency 13 50
sleep 200
# t=600
setConcurrency 14 100
sleep 200
# t=800
setConcurrency 11 10
sleep 200

# stop experiment
kill $pids
wait $pids || true

# done
