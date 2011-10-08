#!/usr/bin/perl
# This script test the capacity of modelnet
# It basically runs the same experiment in the modelnet paper
# The first param is the number of hops in each flow

use Parallel::Jobs;
use XML::DOM;

`rm -f data.txt`;
`rm -f r1.dat result.log`;
$numhops=$ARGV[1];
$numflows=$ARGV[0];

#while ($numflows <=20)
#{
	`rm -f r1.dat`;	
	system("ssh xzong\@sysnet28 'sudo netstat -sw | grep forwarded >> /home/xzong/mnproject/mncapacity/r1.dat'");
	while ($numhops <= 12)
	{
		system("./creategraph.pl ".$numflows." ".$numhops); 
		system("/usr/local/bin/allpairs capacity.graph > capacity.route");  	
		`mkmodel capacity.graph capacity.hosts > capacity.model`;
#`./adjustcapacity.pl $numflows $numhops`;
#`xmlformat.pl capacity.model > capacity1.model`;

		my $parser=new XML::DOM::Parser;
		my $doc=$parser->parsefile("capacity.model");

		my $hosts=$doc->getElementsByTagName("host");
		my $sysnet27=$hosts->item(0);
		my $sysnet26=$hosts->item(1);
		my $vn27 = $sysnet27->getElementsByTagName("virtnode");
		my $vn26 = $sysnet26->getElementsByTagName("virtnode");

		my $port=30000;
		print "++++++++++++++++++++++++++++++++++++++++++++++++++++\n";
		print "Deploy Network Topology !!!!!\n";
		print "====================================================\n";

		`gexec deployhost /home/xzong/mnproject/mncapacity/capacity.model /home/xzong/mnproject/mncapacity/capacity.route`; 	#redeploy modelnet on sysnet27
			for ($i=0; $i<$numflows; $i++)  {
				my $tmp=$i*($numhops+1)+$numhops;
				$pid=Parallel::Jobs::start_job("gexec vnrunhost $tmp /home/xzong/mnproject/mncapacity/capacity.model netserver -p $port");
#$pid=Parallel::Jobs::start_job("vnrun -d $tmp /home/xzong/mnproject/mncapacity/capacity.model iperf -s -p $port -D");
				$port++;
			}

		sleep 1;
		`gexec pkill gexec`;
		print "++++++++++++++++++++++++++++++++++++++++++++++++++++\n";
		print "Server Setup Finished !!!!!\n";
		print "====================================================\n";
		my $port=30000;
		$pid=Parallel::Jobs::start_job("sar 10");
		for ($i=0; $i<$numflows; $i++)  {
			my $tmp=$i*($numhops+1)+$numhops;
			for ($j=0; $j<$vn27->getLength;$j++) {
				if ($vn27->item($j)->getAttribute("int_vn") ==  $tmp) {
					$addr=$vn27->item($j)->getAttribute("vip");
				}
			}
			for ($j=0; $j<$vn26->getLength;$j++) {
				if ($vn26->item($j)->getAttribute("int_vn") ==  $tmp) {
					$addr=$vn26->item($j)->getAttribute("vip");
				}
			}
			$tmp=$tmp-$numhops;
			print $tmp."       ".$addr."\n";
			$pid=Parallel::Jobs::start_job("gexec vnrunhost $tmp /home/xzong/mnproject/mncapacity/capacity.model netperf -H $addr -p $port -l 10 -- -m 1470 -s 128k -S 128k");
#$pid=Parallel::Jobs::start_job("gexec vnrunhost $tmp /home/xzong/mnproject/mncapacity/capacity.model iperf -c $addr -p $port");
			$port++;
		}
		sleep 15;

		print "++++++++++++++++++++++++++++++++++++++++++++++++++++\n";
		print "Finsh Sending!!!!!\n";
		print "====================================================\n";
#`echo "===========================================" >> data.txt`;
		if ($numhops == 8) {
			$numhops=12;
		} else {
			$numhops=$numhops*2;
		}
		`gexec pkill netserver`;
		`gexec pkill netperf`;
		`gexec pkill gexec`;
		`gexec pkill iperf`;
		system("ssh xzong\@sysnet28 'sudo netstat -sw | grep forwarded >> /home/xzong/mnproject/mncapacity/r1.dat'");
		print "++++++++++++++++++++++++++++++++++++++++++++++++++++\n";
		print "Killed All Procs!!!!!\n";
		print "====================================================\n";
		sleep 1;
	}
	`./analyze.pl $numflows`;	
	if ($numflows==1) {
		$numflows=2;
	} elsif ($numflows==2) {
		$numflows=4;
	} else {
		$numflows=$numflows+4;
	}
	print $numflows."\n";
#}


