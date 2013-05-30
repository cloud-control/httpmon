#!/bin/bash

function setCap {
	echo `date +%s` cap=$1
	./actuator --vm rubbos-hvm --cap $1 > /dev/null 2> /dev/null
	sleep 120
}

setCap 800
setCap 200
setCap 400
setCap 300
setCap 600
setCap 500
setCap 200
setCap 800
