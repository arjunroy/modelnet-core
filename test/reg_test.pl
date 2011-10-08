#!/usr/bin/perl
# This script run the regression test on Modelnet Linux version
# It executes three tests
# 1. test the Modelnet emulated pipe bandwidth
# 2. test the Modelnet capacity (which is stated in modelnet paper)
#    http://www.cs.ucsd.edu/~vahdat/papers/modelnet.pdf
# 3. test the ordering of the packets emulated by the Modelnet

# Before running this script, make sure that all the required benchmark tools 
# are installed, like iperf, netperf, gexec, etc.

bw_test();
exit 0;

# TEST 1 -- Bandwidth Test
# We need three machines (one core and two edges) to run the experiment
sub bw_test {

print "======================================\n";
print "Bandwidth Test\n";
print "======================================\n";

print "This test uses one emulator core and two edge clients.\n";
print "Please specify your modelnet emulation core hostname:\n";
chomp(my $server_name= <STDIN>);
print "Please specify your first edge node name:\n";
chomp(my $en1_name= <STDIN>);
print "Please specify your second edge node name:\n";
chomp(my $en2_name= <STDIN>);

chdir 'bwtest' or die "Can't cd to bwtest/: $!\n";

print "Setting hostnames for systems used in test...\n";
system("./svrclient.pl $server_name $en1_name $en2_name");

print "Running the bandwidth test.\n";
system("./udpbwtest.pl $server_name $en1_name $en2_name");
chdir '..';
}

# TEST 2 -- Capacity Test
# ATTENTION: Before running this experiment, please tuning the TCP performance
# like set the TCP buffer size to appropriate value

`rm -rf restult.dat`;
print "======================================\n";
print "Capacity Test\n";
print "======================================\n";

print "Please specify your modelnet core name(ONLY ONE):\n";
chomp(my $server_name= <STDIN>);
print "Please specify your first edge node name:\n";
chomp(my $en1_name= <STDIN>);
print "Please specify your second edge node name:\n";
chomp(my $en2_name= <STDIN>);

chdir 'mncapacity' or die "Can't cd to capacity: $!\n";

`./svrclient.pl $server_name $en1_name $en2_name`;

print "Running Modelnet Capacity Test Script. Please wait...\n";
# run the test script
# system("./testcapacity.pl $server_name");
`./testcapacity.pl $server_name`;


my $diff_res=cap_diff();
if ($diff_res >= 0) {
	print "##############################################\n";
	print "#\n";
	print "#     YOU PASS THE CAPACITY TEST!!!!!!!!\n";
	print "#\n";
	print "##############################################\n";
} else {
	print "##############################################\n";
	print "#\n";
	print "#     YOU FAIL THE CAPACITY TEST!!!!!!!!\n";
	print "#\n";
	print "##############################################\n";
}

# Compare the result with the standard one

chdir '..';


# TEST 3 -- Ordering Test
# We need two machines to run the test
print "======================================\n";
print "Ordering Test\n";
print "======================================\n";
print "Please specify your modelnet core name(ONLY ONE):\n";
chomp(my $server_name= <STDIN>);
print "Please specify your edge node name(ONLY ONE):\n";
chomp(my $client_name= <STDIN>);

chdir 'ordering' or die "Can't cd to ordering: $!\n";

`./svrclient.pl $server_name $client_name`;

# run the test script
#system("./order.pl $server_name");

print "Running Packet Ordering Test Script. Please wait...\n";
`./order.pl $server_name`;

# print out the result
system("cat result.dat");
`pkill iperf`;

chdir '..';

sub bw_diff {
	open (BW_RES, 'bwres.dat') or die("error open the file\n");
	open (STD_BW_RES, 'std_bwres.dat') or die("error open the file\n");

    my @stdbw= <STD_BW_RES>;
	my @bw= <BW_RES>;
	print "hahah       ".(scalar @bw)."\n";
	my $row= scalar @stdbw;
	if ($row != (scalar @bw)) {
		die("Bandwidth test error!!\n");
	}
	for ($i=0; $i < $row; $i++) {
		my $std_data= @stdbw[$i];
		my $data= @bw[$i];
		my $diff=abs ($std_data - $data);
		my $percentage= $diff / $std_data;
		if ($percentage >= 0.2) {
#if the test value if 20% off by the standard value, report it
			print "===================================================\n";
			print "The test result $data Kbps is far beyond the standard result $std_data Kbps.\n";
			print "---------------------------------------------------\n";
			$numerror=$numerror+1;
		}
	}
	if ($numerror >= 5) {
		return -1;
	} else{
		return 0;
	}
}

sub cap_diff {
	open (STD_DATA, 'std_result.dat') or die ("error open the file\n");
	open (DATA, 'result.dat') or die ("error open the file\n");

	# draw the graph first
	`gnuplot draw_compare.plot`;


	my @std_data=<STD_DATA>;
	my @data=<DATA>;


# There are totally seven rows and six columns and we are going to exame
# them one by one

	my $row= scalar @std_data;
	for ($i=0; $i < $row; $i++) {
		my @std_line=split(' ', @std_data[$i]);
		my @line=split(' ', @data[$i]);
		my $col= scalar @std_line;
		for ($j=0; $j < $col; $j++) {
			my $diff=abs (@line[$j] - @std_line[$j]);
			my $percentage= $diff / @std_line[$j];
			if ($percentage >= 0.2) {
#if the test value if 20% off by the standard value, report it
				if ($j == 0) {
					$numhops= 1;
				} elsif ($j ==1) {
					$numhops= 2;
				} elsif ($j ==2) {
					$numhops= 4;
				} elsif ($j ==3) {
					$numhops= 8;
				} else {
					$numhops= 12;
				}
				$numflows=@line[5];
				print "===================================================\n";
				print "echo The test result @line[$j] is far beyond the standard result @std_line[$j], while sending $numflows TCP flows along the path with $numhops pipes.\n";
				print "---------------------------------------------------\n";
				$numerror=$numerror+1;
			}
		}
	}
	if ($numerror >= 5) {
		return -1;
	} else{
		return 0;
	}
}
