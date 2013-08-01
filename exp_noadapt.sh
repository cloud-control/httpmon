#!/bin/bash

for serviceLevel in 0.0 0.25 0.5 0.75 1.0; do
	for patternId in 1 2 3 4 5 6 7 8 9 0; do
		./exp_pattern.sh 0 $serviceLevel $patternId
	done
done

echo "Dear Madam or Sir. Your experiment is ready." | mail -s "Done" cristiklein@gmail.com

