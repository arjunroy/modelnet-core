# This file contains the instruction for GNUPlot to plot the graph for comparing the current result from the standard one

set term png
set output "compare.png"
set title "ModelNet Scalability Test (Compare Version)"
set xlabel "Number of Flows (500Mbps per hop)"
set ylabel "Number of Packets Forwarded by the core"
plot "std_result.dat" using 6:5 title "12 hop (Standard)" with linespoints, \
"std_result.dat" using 6:4 title "8 hops (Standard)" with linespoints, \
"std_result.dat" using 6:3 title "4 hops (Standard)" with linespoints, \
"std_result.dat" using 6:2 title "2 hops (Standard)" with linespoints, \
"std_result.dat" using 6:1 title "1 hops (Standard)" with linespoints, \
"result.dat" using 6:5 title "12 hop" with linespoints, \
"result.dat" using 6:4 title "8 hops" with linespoints, \
"result.dat" using 6:3 title "4 hops" with linespoints, \
"result.dat" using 6:2 title "2 hops" with linespoints, \
"result.dat" using 6:1 title "1 hops" with linespoints
