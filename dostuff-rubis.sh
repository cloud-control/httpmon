#!/bin/bash

function setCap {
	echo `date +%s` cap=$1
	./actuator --vm rubis-hvm --cap $1 > /dev/null 2> /dev/null
	sleep 120
}

setCap 400
setCap 200
setCap  50
setCap 300
setCap 100
setCap 200
setCap  50
setCap 400
