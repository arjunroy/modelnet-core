#!/usr/bin/perl
# This script takes stat output from perfstats and removes rows representing no communication before and after the run.

$first = 1;
$thresh=$ARGV[1];
while (<STDIN>)
  {
    if ($first == 1) { $first = 2; next; }
    ($t, $cpu, $pkts_seen) = split ;
    if ($pkts_seen <= $thresh || $pkts_seen >=500000)
      {
	if (first == 3) {
	  break;
	}
      }
    else
      {
	$first = 3;
	print ;
      }
  }
