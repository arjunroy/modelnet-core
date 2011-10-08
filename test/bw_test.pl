#!/usr/bin/perl
# This script run the bandwidth test on Modelnet Linux version

# Before running this script, make sure that all the required benchmark tools 
# are installed, like iperf, netperf, gexec, etc.

use Cwd;
use strict;
use warnings;

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

my $bw_test_dir = getcwd;
my $bw_test_modelfile = "$bw_test_dir/bwtest1.model";
my $bw_test_routefile = "$bw_test_dir/bwtest1.route";
my $data_dir = "$bw_test_dir/output";

print "Setting hostnames for systems used in test...\n";
system("./svrclient.pl $server_name $en1_name $en2_name");

print "Running the bandwidth test.\n";
system("./udpbwtest.pl $server_name $en1_name $en2_name $bw_test_modelfile $bw_test_routefile $data_dir");
chdir '..';
}

