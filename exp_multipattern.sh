#!/bin/bash

pattern="100 150 150 100 150 100 050 050 150 100"
./exp_pattern.sh 0.5 0.5 "$pattern" 
./exp_pattern.sh 0.9 0.5 "$pattern" 

pattern="200 250 250 200 250 200 150 150 250 200"
./exp_pattern.sh 0.5 0.5 "$pattern" 
./exp_pattern.sh 0.9 0.5 "$pattern" 

pattern="400 450 450 400 450 400 350 350 450 400"
./exp_pattern.sh 0.5 0.5 "$pattern" 
./exp_pattern.sh 0.9 0.5 "$pattern" 

pattern="600 650 650 600 650 600 550 550 650 600"
./exp_pattern.sh 0.5 0.5 "$pattern" 
./exp_pattern.sh 0.9 0.5 "$pattern" 

pattern="800 50 800 50 800 50 800 50 800 50"
./exp_pattern.sh 0.5 0.5 "$pattern" 
./exp_pattern.sh 0.9 0.5 "$pattern" 

pattern="50 800 50 800 50 800 50 800 50 800"
./exp_pattern.sh 0.5 0.5 "$pattern" 
./exp_pattern.sh 0.9 0.5 "$pattern" 

pattern="400 800 50 400 200 600 200 400 800 400"
./exp_pattern.sh 0.5 0.5 "$pattern" 
./exp_pattern.sh 0.9 0.5 "$pattern" 

pattern="250 75 125 85 650 575 275 750 75 375"
./exp_pattern.sh 0.5 0.5 "$pattern" 
./exp_pattern.sh 0.9 0.5 "$pattern" 

pattern="325 625 650 200 425 385 525 575 600 250"
./exp_pattern.sh 0.5 0.5 "$pattern" 
./exp_pattern.sh 0.9 0.5 "$pattern" 

pattern="675 475 450 775 750 600 500 325 475 100"
./exp_pattern.sh 0.5 0.5 "$pattern" 
./exp_pattern.sh 0.9 0.5 "$pattern" 

echo "Dear Madam or Sir. Your experiment is ready." | mail -s "Done" maggio.martina@gmail.com cristiklein@gmail.com

