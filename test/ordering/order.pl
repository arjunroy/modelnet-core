#!/usr/bin/perl

use Cwd;
my $PWD= getcwd;

`rm -f result.dat`;
my $bw=100;
open(RES, ">>result.dat") or die("ERROR\n");

print RES "######################################################\n";
while ($bw <= 100000) {

	`./bwmodify.pl $bw`;

	`gexec deployhost $PWD/order.model $PWD/order.route`; 	#redeploy modelnet on sysnet27
		`/usr/local/bin/vnrun -d 0 $PWD/order.model iperf -s -u &`;	# run iperf server
		system("/usr/local/bin/vnrunhost 1 $PWD/order.model iperf -c 10.0.0.1 -u -b ".$bw.'k');
	`sleep 1`;
	`pkill iperf`;

	my $server=$ARGV[0];
	`ssh xzong\@$server 'cat /proc/order_in' > test`;

	open (TEST, "test") or die("ERROR\n");
	my @line=<TEST>;
	my $len=length @line[0]; 
	if ($len == 0 || $len == 1) {
		print RES "[PASS] the ordering test with pipe bandwidth $bw(Kbps).\n";
	} else {
		print RES "[FAILED] the ordering test with pipe bandwidth $bw(Kbps).\n";
	}
	$bw=$bw*10;
	close(TEST);
}
print RES "######################################################\n";

close(RES);
`rm -f test`;
