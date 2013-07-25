#!/bin/bash

for serviceLevel in 0.0 0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9 1.0; do
	for patternId in 1 2 3 4 5 6 7 8 9 0; do
		./exp_pattern.sh 0 $serviceLevel C
	done
done
./exp_pattern.sh 0.5 0.5 C

echo "Dear Madam or Sir. Your experiment is ready." | mail -s "Done" maggio.martina@gmail.com cristiklein@gmail.com

