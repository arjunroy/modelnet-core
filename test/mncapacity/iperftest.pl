#!/usr/bin/perl


use Parallel::Jobs;

$temp=$ARGV[0];
$pid=Parallel::Jobs::start_job("sar 10");
$pid=Parallel::Jobs::start_job("iperf -c sysnet28 -u -b 200m -l $temp");
#for ($i=0; $i<20; $i++) {
#	$pid=Parallel::Jobs::start_job("iperf -c sysnet28 -u -b 200m $ARGV[0]");
#	$pid=Parallel::Jobs::start_job("iperf -c sysnet28 -u -b 10m");
#	$pid=Parallel::Jobs::start_job("netperf -H sysnet28 -t UDP_STREAM");
#}
sleep 200;
