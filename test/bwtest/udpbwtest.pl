#!/usr/bin/perl

# This script is run on any of the modelnet emulator nodes, or on one of the client nodes.
# The following actions are performed for each value of hop bandwidth:
# 1. Deploy the modelnet setup on all of the emulator and client nodes.
#    For this step we assume gexec and authd is properly setup on all clients and all emulators.
#    We also check the environment variable MODELNET_PREFIX and GEXEC_PREFIX to find the bin/
#    and sbin/ folders where the executables reside. If they are not set, we default to
#    /usr/local as the prefix.
# 2. Start a UDP Iperf server on machine 1 (of 2).
# 3. For values that are 0.4, 0.6, 0.8, 1.0, 1.2 and 1.4 times the current set hop bandwidth,
#    start an iperf instance on machine 2 (of 2) that connects to the iperf server on machine 1
#    and write out the results of that client to file.
# 4. Parse the results file to get a data file and a gnuplot file.
# 5. Create the gnuplot.

# The range of values for bandwidth limiting is from 100 kbps to 10,000,000 kbps.
# Note that the entry point, do_bw_test, requires the hostnames for the emulator host,
# edge client one and edge client two, in that order.

use Cwd;
use Env qw ($MODELNET_PREFIX $GEXEC_PREFIX);
use BWModify qw (change_modelfile_bw);

use strict;
use warnings;

###############################################################################
# Main function
#
# Arguments from ARGV: 
# emulator_host : hostname for emulator
# client_one : hostname for client one
# client_two : hostname for client two
# bw_test_modelfile : model file for bandwidth test
# bw_test_routefile : route file for bandwidth test
# data_dir : where output is stored
#
# Optional arguments from ENV: 
# MODELNET_PREFIX : Prefix for modelnet related executables and libraries
# Defaults to /usr/local if not specified.
# GEXEC_PREFIX : Prefix for gexec and authd related executables and libraries
# Defaults to /usr/local if not specified.
#
# Script runs bandwidth test and creates gnuplots based on data.
###############################################################################

# XXX Tweak the following as desired.
my $hopbw_kbps = 1000000;
my $maxhopbw_kbps = 1000000;
#my @udp_bw_steps = (0.1, 0.2, 0.4, 0.6, 0.8, 1.0, 1.2, 1.4);
my @udp_bw_steps = (1.0);
my $iperf_output_file_name = 'iperf_output.txt';
my $data_file_name = 'output.data';
my $udp_throughput_data_file_name = 'throughput.data';
my $increment = $hopbw_kbps;
my $gnuplot_file_name = 'plot.p';

# For gexec, we switch to /tmp before running anything because gexec
# complains and doesn't do anything if the target system does not have
# our current working dir in its filesystem (ie. it tries a chdir first
# when it runs). All this means is that we don't use relative paths.
# Also, we assume any system we run on has /tmp.
my $tmp_dir = '/tmp';

# Prefix variables come from environment, otherwise we fallback.
my $MODELNET_PREFIX = $ENV{'MODELNET_PREFIX'};
my $GEXEC_PREFIX = $ENV{'GEXEC_PREFIX'};

if (!$MODELNET_PREFIX) {
    $MODELNET_PREFIX = '/usr/local';
}
if (!$GEXEC_PREFIX) {
    $GEXEC_PREFIX = '/usr/local';
}

# Set filenames for executables we use.
my $deploy_cmd = "$MODELNET_PREFIX/bin/deploy";
my $gexec_cmd = "$GEXEC_PREFIX/bin/gexec";

# Get command line parameters.
(my $emulator_host, 
 my $client_one, 
 my $client_two, 
 my $bw_test_modelfile,
 my $bw_test_routefile,
 my $data_dir) = @ARGV;

# Definitions for remote commands that we will use.
# For passing environment variables to the remote end, just use sh -c 'ENVVAR=foo /bin/bar'
# This is used to set LD_PRELOAD and SRCIP for the libipaddr.so interposition.
my $iperf_server_cmd = "LD_PRELOAD=${MODELNET_PREFIX}/lib/libipaddr.so SRCIP=10.0.0.1 iperf -s -u";
my $gexec_server_cmd = "GEXEC_SVRS=\"${client_one}\" $gexec_cmd sh -c '$iperf_server_cmd'&";
my $gexec_pkill_cmd = "GEXEC_SVRS=\"${client_one}\" $gexec_cmd pkill iperf";

# Run the actual test.
do_bw_test();

###############################################################################
# End of main function
###############################################################################

# Function do_bw_test(emulator_host, client_one, client_two)
# Do UDP bandwidth test as described above, where the Iperf server
# is run on client one and the Iperf client is run on client two.
# Note that the IP addresses involved are hardcoded based on the
# model file.
sub do_bw_test {
    # Make output directory if needed.
    if (! -d $data_dir) {
        die "Can't make directory $data_dir\n" unless mkdir $data_dir;
    }

    # Prepare the UDP throughput test.
    my $udp_throughput_data_file = "$data_dir/$udp_throughput_data_file_name";
    prepare_udp_throughput_test($udp_throughput_data_file);

    # Run test for each desired iteration.
    while ($hopbw_kbps <= $maxhopbw_kbps)
    {
        print "Running bandwidth test with bandwidth $hopbw_kbps kbps\n";

         # Make directory for this iteration. 
        my $current_run_dir = "$data_dir/$hopbw_kbps"."_data";
        if (! -d $current_run_dir) {
            die "Can't mkdir $current_run_dir\n" unless mkdir $current_run_dir;
        }

        # Do modelnet deploy.
        do_modelnet_deploy($hopbw_kbps);

        # Run the constant bandwidth test at this link bandwidth.    
        do_udp_constant_bw_test($hopbw_kbps, $current_run_dir);

        # Run the portion of the throughput scaling test for this link bandwidth.
        do_udp_throughput_test_portion($hopbw_kbps, $udp_throughput_data_file);

        print "Done running bandwidth test with bandwidth $hopbw_kbps kbps\n";

        # We're basically increasing link bandwidth in a log-scale sort of way.
        if ($hopbw_kbps >= 10 * $increment) {
            my $old_incr = $increment;
            $increment *= 10;
            print "Changing increment to $increment from $old_incr\n";
        }
        $hopbw_kbps += $increment;
    }

    # Process and finalize results for UDP throughput test.
    finalize_udp_throughput_test($udp_throughput_data_file);
}

# function do_gexec(cmd)
# Backs up current directory, chdir to tmp, runs command, and chdir to backed up dir.
# We do this so gexec doesn't complain when the cwd doesn't exist on remote end.
sub do_gexec {
    (my $remote_cmd) = @_;
    my $backup_dir = getcwd;
    die "Couldn't change to $tmp_dir before running gexec!\n" unless chdir $tmp_dir;
    
    system($remote_cmd);

    die "Couldn't change back to $backup_dir after running gexec!\n" unless chdir $backup_dir;
}

# Modify the link bandwidth and redeploy modelnet on all emulators and clients.
sub do_modelnet_deploy {
    (my $hopbw_kbps) = @_;

    # Edit model file to have the specified link bandwidth. 
    die "Unable to edit modelfile kbps!\n" unless change_modelfile_bw($bw_test_modelfile, $hopbw_kbps);

    # Deploy the modelnet topology.
    my $cmd = "GEXEC_SVRS=\"$emulator_host $client_one $client_two\" $deploy_cmd $bw_test_modelfile $bw_test_routefile";
    do_gexec($cmd);
}

# function prepare_udp_throughput_test(udp_throughput_data_file)
#
# Clears results and data of UDP throughput test from previous full run
# of the UDP bandwidth test script.
sub prepare_udp_throughput_test {
    (my $data_file) = @_;
    
    # Delete data file from last full run of script.
    unlink $data_file;

    # Print header for new data file.
    open (DATA, ">>$data_file") or die "Error opening data file $data_file.\n";
    print DATA "\# Hop_bandwidth(kbps) Generated_UDP_Rate(kbps) Observed_Throughput(kbps)\n";
    close(DATA);
}

# function do_udp_throughput_test_portion(hopbw_kbps, udp_throughput_data_file)
#
# Runs iperf server and client test using UDP where client generation
# rate is equal to link bandwidth. Appends result to file for graph
# generation in function finalize_udp_throughput_test.
sub do_udp_throughput_test_portion {
    (my $hopbw_kbps, my $data_file) = @_;

    # Delete raw output from last iteration.
    my $raw_file = "$data_dir/$iperf_output_file_name";
    unlink $raw_file;

    # Setup client command. 
    my $udp_bw_kbps = $hopbw_kbps;
    my $iperf_client_cmd = "LD_PRELOAD=${MODELNET_PREFIX}/lib/libipaddr.so SRCIP=10.0.0.5 iperf -c 10.0.0.1 -u -fk -b ${udp_bw_kbps}k";
    my $gexec_client_cmd = "GEXEC_SVRS=\"${client_two}\" $gexec_cmd sh -c '$iperf_client_cmd' > $raw_file";

    # Start the iperf server
    do_gexec($gexec_server_cmd);

    # Run the client.
    do_gexec($gexec_client_cmd);

    # Kill the iperf server
    # Why 2 pkills? Because iperf server wants to kills before actually quitting.
    do_gexec($gexec_pkill_cmd);
    do_gexec($gexec_pkill_cmd);

    # Parse raw output file and modify data file accordingly.
    parse_iperf_output_to_data($hopbw_kbps, $udp_bw_kbps, $raw_file, $data_file);
}

# function finalize_udp_throughput_test(udp_throughput_data_file)
#
# Creates gnuplot of observed throughput vs. modelnet set throughput
# using data collected in function do_udp_throughput_test_portion().
sub finalize_udp_throughput_test {
    (my $data_file) = @_;
    plot_throughput_scaling_graph($data_file);
}

# function do_udp_constant_bw_test(hopbw_kbps, current_run_dir)
#
# Hold bandwidth constant, and test UDP throughput at the given
# percentages of link bandwidth specifed in @udp_bw_steps.
# Generates one graph at a given bandwidth plotting observed UDP throughput
# at the server as a function of generated UDP throughput at the client.
sub do_udp_constant_bw_test {
    (my $hopbw_kbps, my $current_run_dir) = @_;

    my $raw_file = "$current_run_dir/$iperf_output_file_name";
    my $data_file = "$current_run_dir/$data_file_name";

    # Delete data file from last full run of bandwidth test.
    unlink $data_file;

    # Print header for new data file.
    open (DATA, ">>$data_file") or die "Error opening data file $data_file.\n";
    print DATA "\# Hop_bandwidth(kbps) Generated_UDP_Rate(kbps) Observed_Throughput(kbps)\n";
    close(DATA);

    for my $udp_bw_modifier (@udp_bw_steps) {
        my $udp_bw_kbps = $hopbw_kbps * $udp_bw_modifier;
        my $iperf_client_cmd = "LD_PRELOAD=${MODELNET_PREFIX}/lib/libipaddr.so SRCIP=10.0.0.5 iperf -c 10.0.0.1 -u -fk -b ${udp_bw_kbps}k";
        my $gexec_client_cmd = "GEXEC_SVRS=\"${client_two}\" $gexec_cmd sh -c '$iperf_client_cmd' > $raw_file";

        # Delete raw output file for last run of iperf.
        unlink $raw_file;
 
        # Start the iperf server
        do_gexec($gexec_server_cmd);

        # Run the client.
        do_gexec($gexec_client_cmd);

        # Kill the iperf server
        # Why 2 pkills? Because iperf server wants to kills before actually quitting.
        do_gexec($gexec_pkill_cmd);
        do_gexec($gexec_pkill_cmd);

        # Parse raw output file and modify data file accordingly.
        parse_iperf_output_to_data($hopbw_kbps, $udp_bw_kbps, $raw_file, $data_file);
    }

    # Generate gnuplot from datafile
    plot_constant_bandwidth_graph($hopbw_kbps, $current_run_dir, $data_file);

    print "Done with test at bandwidth $hopbw_kbps\n";
}

sub plot_throughput_scaling_graph {
    (my $data_file) = @_;

    my $gnuplot_file = "$data_dir/$gnuplot_file_name";
    my $graph_file = "$data_dir/throughput_scaling.ps";

    open(PLOTSCRIPT, ">$gnuplot_file") or die "Error opening gnuplot file $gnuplot_file.\n";
    print PLOTSCRIPT "set terminal postscript\n";
    print PLOTSCRIPT "set xtic auto\n";
    print PLOTSCRIPT "set ytic auto\n";
    print PLOTSCRIPT "set title \"Observed UDP Throughput vs. Modelnet link bandwidth\"\n";
    print PLOTSCRIPT "set xlabel \"Modelnet link bandwidth (kbps)\"\n";
    print PLOTSCRIPT "set ylabel \"UDP Throughput at Iperf Server (kbps)\"\n";
    print PLOTSCRIPT "set output \"$graph_file\"\n";
    print PLOTSCRIPT "plot \"$data_file\" using 1:3 title 'Throughput (kbps)' with linespoints\n";
    close(PLOTSCRIPT); 

    system("gnuplot $gnuplot_file");
}

# function plot_constant_bandwidth_graph(hopbw_kbps, current_run_dir, data_file)
#
# Creates plot of observed UDP throughput at iperf server vs.
# generation rate at iperf client for a fixed link bandwidth.
sub plot_constant_bandwidth_graph {
    (my $hopbw_kbps, my $current_run_dir, my $data_file) = @_;

    my $gnuplot_file = "$current_run_dir/$gnuplot_file_name";
    my $graph_file = "$current_run_dir/${hopbw_kbps}_kbps_constant_bandwidth.ps";

    open(PLOTSCRIPT, ">$gnuplot_file") or die "Error opening gnuplot file $gnuplot_file.\n";
    print PLOTSCRIPT "set terminal postscript\n";
    print PLOTSCRIPT "set xtic auto\n";
    print PLOTSCRIPT "set ytic auto\n";
    print PLOTSCRIPT "set title \"Observed UDP Throughput vs. UDP Generation Rate over $hopbw_kbps kbps link\"\n";
    print PLOTSCRIPT "set xlabel \"UDP Generation Rate (kbps)\"\n";
    print PLOTSCRIPT "set ylabel \"UDP Throughput at Iperf Server (kbps)\"\n";
    print PLOTSCRIPT "set output \"$graph_file\"\n";
    print PLOTSCRIPT "plot \"$data_file\" using 2:3 title 'Throughput (kbps)' with linespoints\n";
    close(PLOTSCRIPT); 

    system("gnuplot $gnuplot_file");
}

# function parse_iperf_output_to_data(hopbw_kbps, generated_bpbs, raw_file, data_file)
#
# Parses raw file containing output from iperf to get the server
# visible throughput. Appends data to data file.
sub parse_iperf_output_to_data {
    (my $hopbw_kbps, my $generated_kbps, my $raw_file, my $data_file) = @_;

    open (RAW, $raw_file) or die "Error opening iperf output in $raw_file.\n";
    open (DATA, ">>$data_file") or die "Error opening data file $data_file.\n";

    my $nextisdata = 0;
    foreach my $line (<RAW>) {
        my @tokens= split(' ', $line);
        if (scalar (@tokens) > 3 && $tokens[3] eq 'Server') {
            $nextisdata=1;
        }
        elsif ($nextisdata) {
            # This line contains the actual throughput we want
            my $throughput= $tokens[7];
            if ($throughput eq 'MBytes' || $throughput eq 'KBytes') {
                $throughput= $tokens[8];
            }
            print DATA "$hopbw_kbps $generated_kbps $throughput\n";
            $nextisdata=0;
        }
    }

    close(RAW);
    close(DATA);
}
 
