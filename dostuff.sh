#!/bin/sh
date +%s
./actuator --vm rubis-hvm --cap 400
sleep 60

date +%s
./actuator --vm rubis-hvm --cap 200
sleep 60

date +%s
./actuator --vm rubis-hvm --cap 100
sleep 60

date +%s
./actuator --vm rubis-hvm --cap 50
sleep 60

date +%s
./actuator --vm rubis-hvm --cap 100
sleep 60

date +%s
./actuator --vm rubis-hvm --cap 200
sleep 60

date +%s
./actuator --vm rubis-hvm --cap 400
sleep 60
