# This file contains the instruction for GNUPlot to plot the graph for modelnet scalability test

set term png
set output "scal.png"
set title "Modelnet Scanlability Test"
set xlabel "Number of Flows (50Mbps per hop)"
set ylabel "Number of Packets Forwarded by the core"
plot "capacityplot.dat" using 6:5 title "12 hop" with linespoints, \
"capacityplot.dat" using 6:4 title "8 hops" with linespoints, \
"capacityplot.dat" using 6:3 title "4 hops" with linespoints, \
"capacityplot.dat" using 6:2 title "2 hops" with linespoints, \
"capacityplot.dat" using 6:1 title "1 hops" with linespoints
