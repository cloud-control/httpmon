#!/bin/bash

for pole in 0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9; do
	for patternId in 1 2 3 4 5 6 7 8 9 0; do
		./exp_pattern.sh $pole 0.5 $patternId
	done
done

echo "Dear Madam or Sir. Your experiment is ready." | mail -s "Done" maggio.martina@gmail.com cristiklein@gmail.com

