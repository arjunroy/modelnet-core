#!/usr/bin/env bash

## Run this like this
## ./make_graphs.sh static1 ; cat /tmp/grant.gnuplot | gnuplot


expdir=$1
#my_n=$2

if [ ! $expdir ]; then
  echo "usage $0 <expdir>" 1>&2
  exit 1
fi

#if [ ! "$my_n" ]; then
#  my_n="*"
#fi

gfile=/tmp/`whoami`.gnuplot
rm -f $gfile

cat <<END >>$gfile

set terminal postscript "Times-Roman" 28
#set title  'cpu utilization'
set xlabel 'cores' 
set ylabel 'CPU utilization' 
set xrange[0:8]
set yrange[0:1]
set output "cpu_${expdir}.eps"
set key left bottom
set data style points
plot "rand320_sum.txt" using 1:2 title 'random' with lines, \
	   "blob320_sum.txt" using 1:2 title 'k-cluster' with lines, \
	   "met320_sum.txt" using 1:2 title 'metis' with lines 


#set terminal postscript
#set title  'efficiency'
set xlabel 'cores'
set ylabel 'efficiency'
set xrange[0:8]
set yrange[0:1]
set output "efficiency_${expdir}.eps"
set key left bottom
set data style points
plot "rand320_sum.txt" using 1:9 title 'random' with lines, \
	   "blob320_sum.txt" using 1:9 title 'k-cluster' with lines, \
           "met320_sum.txt" using 1:9 title 'metis' with lines

#set terminal postscript
#set title  'cpu utilization'
set xlabel 'cores'
set ylabel 'total pps'
set xrange[0:8]
set autoscale y
set output "pps_${expdir}.eps"
set key right bottom
set data style points
plot "rand320_sum.txt" using 1:3 title 'random' with lines, \
	   "blob320_sum.txt" using 1:3 title 'k-cluster' with lines, \
	   "met320_sum.txt" using 1:3 title 'metis' with lines

set terminal postscript
set xlabel 'cores'
set ylabel 'physical hops / emulated hops'
set xrange[0:8]
set autoscale y
set output "intra_${expdir}.eps"
set key right bottom
set data style points
plot "rand320_sum.txt" using 1:10 title 'random' with lines, \
	   "blob320_sum.txt" using 1:10 title 'k-cluster' with lines, \
	   "met320_sum.txt" using 1:10 title 'metis' with lines


set terminal postscript
set xlabel 'cores'
set ylabel 'CPU standard deviation'
set xrange[0:8]
set autoscale y
set output "stddev_${expdir}.eps"
set key right bottom
set data style points
plot "rand320_sum.txt" using 1:11 title 'random' with lines, \
	   "blob320_sum.txt" using 1:11 title 'k-cluster' with lines, \
	   "met320_sum.txt" using 1:11 title 'metis' with lines



# this is the expected gain in bandwidth 
set terminal postscript
set xlabel 'cores'
set ylabel 'Expected Capacity Benefit (pps)'
set xrange[0:8]
set autoscale y
set output "expected_${expdir}.eps"
set key right bottom
set data style points
plot "expected_table.txt" using 1:3 title 'metis' with lines, \
"expected_table.txt" using 1:2 title 'k-cluster' with lines





