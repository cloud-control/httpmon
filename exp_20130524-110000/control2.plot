fn = 'control2.data'
t0 = 1369387608

set logscale y
set ytics nomirror
set y2tics

set xlabel "Experiment time"
set ylabel "Latency (ms)" tc lt 1
set y2label "Recommendation rate (%)" tc lt 2

t = 1369387608
cap = '400'
set arrow nohead from first t-t0,graph 0 to first t-t0, graph 1
set label cap at first t-t0,graph 1 offset 0,-0.4

t = 1369387668
cap = '200'
set arrow nohead from first t-t0,graph 0 to first t-t0, graph 1
set label cap at first t-t0,graph 1 offset 0,-0.4

t = 1369387729
cap = '100'
set arrow nohead from first t-t0,graph 0 to first t-t0, graph 1
set label cap at first t-t0,graph 1 offset 0,-0.4

t = 1369387789
cap = '50'
set arrow nohead from first t-t0,graph 0 to first t-t0, graph 1
set label cap at first t-t0,graph 1 offset 0,-0.4

t = 1369387850
cap = '100'
set arrow nohead from first t-t0,graph 0 to first t-t0, graph 1
set label cap at first t-t0,graph 1 offset 0,-0.4

t = 1369387910
cap = '200'
set arrow nohead from first t-t0,graph 0 to first t-t0, graph 1
set label cap at first t-t0,graph 1 offset 0,-0.4

t = 1369387971
cap = '400'
set arrow nohead from first t-t0,graph 0 to first t-t0, graph 1
set label cap at first t-t0,graph 1 offset 0,-0.4

set arrow nohead from graph 0,first 500 to graph 1, first 500

set title "100 clients with think time 1 s"

plot \
	fn u ($1-t0):3 t "Latency" lt 1, \
	fn u ($1-t0):5 axis x1y2 t "Recommendation rate" lt 2

