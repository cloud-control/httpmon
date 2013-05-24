fn = 'control.data'
t0 = 1369384296.390645

set logscale y
set ytics nomirror
set y2tics

set xlabel "Experiment time"
set ylabel "Latency (ms)" tc lt 1
set y2label "Recommendation rate (%)" tc lt 2

t = 1369384301
cap = '400'
set arrow nohead from first t-t0,graph 0 to first t-t0, graph 1
set label cap at first t-t0,graph 1 offset 0,-0.4

t = 1369384362
cap = '200'
set arrow nohead from first t-t0,graph 0 to first t-t0, graph 1
set label cap at first t-t0,graph 1 offset 0,-0.4

t = 1369384422
cap = '100'
set arrow nohead from first t-t0,graph 0 to first t-t0, graph 1
set label cap at first t-t0,graph 1 offset 0,-0.4

t = 1369384483
cap = '50'
set arrow nohead from first t-t0,graph 0 to first t-t0, graph 1
set label cap at first t-t0,graph 1 offset 0,-0.4

t = 1369384543
cap = '100'
set arrow nohead from first t-t0,graph 0 to first t-t0, graph 1
set label cap at first t-t0,graph 1 offset 0,-0.4

t = 1369384604
cap = '200'
set arrow nohead from first t-t0,graph 0 to first t-t0, graph 1
set label cap at first t-t0,graph 1 offset 0,-0.4

t = 1369384665
cap = '400'
set arrow nohead from first t-t0,graph 0 to first t-t0, graph 1
set label cap at first t-t0,graph 1 offset 0,-0.4

set arrow nohead from graph 0,first 500 to graph 1, first 500

set title "100 clients with think time 0.1s"

plot \
	fn u ($1-t0):3 t "Latency" lt 1, \
	fn u ($1-t0):5 axis x1y2 t "Recommendation rate" lt 2

