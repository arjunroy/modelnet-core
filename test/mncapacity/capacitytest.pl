#!/usr/bin/perl
# Runs a capacity test on Modelnet in the following manner:
# The test is run with one modelnet core and one or more modelnet end hosts.
# The topology consists of virtual nodes arranged in a long chain, where
# each virtual node has exactly two neighbors except the first and last which
# have a single neighbor each.
#
# This test aims to test the effect that increasing the number of hops for
# flows in the network has on the scalability of Modelnet in terms of how many
# packets can be routed by the modelnet core, and the aggregate throughput.
# Each flow gets exclusive access to a subset of the total set of hops in the
# topology.
#
# In particular, if there are H hops that each flow must traverse, and there
# are F flows in total, then the total number of hops is F*H, while the total
# number of virtual nodes N in the topology is (F*H)+1.
# 
# Let each flow be indexed from [0, F-1]. Then flow f originates at virtual
# node f*H and has the destination set to virtual node (f+1)*H.
# We vary the number of hops and the number of flows, but keep the bandwidth of
# each hop fixed. The parameters can be configured as desired.
#
# We result in four types of graphs: packets forwarded (aggregate throughput) 
# as a function of number of hops for a given number of flows, and packets 
# forwarded (aggregate throughput) as a function of number of flows with 
# multiple series for each value for number of hops.

use strict;
use warnings;

use threads;
use Cwd;
use File::Spec;

use XML::DOM;
use CapacityTest::CreateGraph qw (createtopology);
use CapacityTest::Util qw (get_tcp_flow_throughput_mbps get_ip_packets_forwarded_by_host_count);

use CapacityTest::Graph qw (plot_tput_vs_hops_data_per_numflows plot_fwd_vs_hops_data_per_numflows plot_tput_vs_flows_data_per_numhops plot_fwd_vs_flows_data_per_numhops);

use Data::Dumper;

# Prefix variables come from environment, otherwise we fallback.
my $MODELNET_PREFIX = $ENV{'MODELNET_PREFIX'};
my $GEXEC_PREFIX = $ENV{'GEXEC_PREFIX'};

if (!$MODELNET_PREFIX) {
      $MODELNET_PREFIX = '/usr/local';
}
if (!$GEXEC_PREFIX) {
      $GEXEC_PREFIX = '/usr/local';
}

# Link parameters
my $bw_kbps = 100000;
my @hop_values = (1, 2, 4, 8, 12);
my @flow_values = (1, 2, 4, 8, 12, 16, 20);
my $base_port = 30000;
my $drop_rate = 0;
my $delay_ms = 1;
my $queue_len = 4000;

my $iperf_runtime = 10;
my $sleep_delay = 5;

# Filenames
my $graph_filename = 'capacity.graph';
my $route_filename = 'capacity.route';
my $model_filename = 'capacity.model';
my $output_dir = 'output';
my $graph_dir = 'graphs';

# Used for redirecting output as necessary
# for example: when we get #IP packets forwarded by emulator with netstat -sw
my $tmp_file = 'tmp_file';

# Commands we use
my $allpairs_cmd = "$MODELNET_PREFIX/bin/allpairs";
my $mkmodel_cmd = "$MODELNET_PREFIX/bin/mkmodel";
my $deploy_cmd = "$MODELNET_PREFIX/bin/deploy";
my $vnrun_cmd = "$MODELNET_PREFIX/bin/vnrun";
my $vnrunhost_cmd = "$MODELNET_PREFIX/bin/vnrunhost";
my $gexec_cmd = "$GEXEC_PREFIX/bin/gexec";

###############################################################################
# Entry point (Main method)
###############################################################################

if (scalar @ARGV != 2) {
    print "Usage: ./capacitytest.pl <hosts_file> <emulator_hostname>\n";
    exit 1;
}

(my $hosts_file_relative_path, my $emulator_name) = @ARGV;
my $hosts_file = File::Spec->rel2abs($hosts_file_relative_path);
run_capacity_test($hosts_file, $emulator_name);
exit 0;

###############################################################################
# End of entry point
###############################################################################

sub run_capacity_test
{
    (my $hosts_file, my $emulator_name) = @_;

    # Store four structures with data for graphs
    # The first two are maps from #flows to a table 
    # containing (#hops, throughput/forwarded) entries.
    # This will be used for graphing capacity vs #hops
    # for a fixed number of flows (tput_vs_hops_data_per_numflows
    # and fwd_vs_hops_data_per_numflows).
    #
    # The second is a map from #hops to a table 
    # containing (#flows, throughput/fowraded) entries.
    # This will be used to graph capacity vs #flows with
    # a series for each fixed #hops (tput_vs_flows_data_per_numhops
    # and fwd_vs_flows_data_per_numhops).
    #
    # Each table described above is also implemented 
    # as a map with either #hops or #flows as key.
    my $tput_vs_hops_data_per_numflows = {};
    my $tput_vs_flows_data_per_numhops = {};
    my $fwd_vs_hops_data_per_numflows = {};
    my $fwd_vs_flows_data_per_numhops = {};

    # Delete all old data
    system("rm -rf ./$output_dir");

    # Recreate output directory
    die "Could not create $output_dir directory.\n" unless mkdir $output_dir;

    # Change directory
    my $top_level_dir = getcwd;
    die "Could not switch directory to $output_dir.\n" unless chdir $output_dir;
    my $output_level_dir = getcwd;

    for my $numflows (@flow_values) {
        for my $numhops (@hop_values) {
            # Set up per test directory
            my $testdir = "test_${numflows}_flows_${numhops}_hops_data";
            die "Could not make directory $output_dir/$testdir.\n" unless mkdir $testdir;
            die "Could not switch directory to $output_dir/$testdir.\n" unless chdir $testdir;

            # Run test
            print "Running capacity test with $numhops hops and $numflows flows.\n";
            my $results = do_capacity_test($emulator_name, $hosts_file, $bw_kbps, $numhops, $numflows);
            my $forwarded = $results->{'FORWARDED'};
            my $throughput = $results->{'THROUGHPUT'};

            # Store data
            my $tput_vs_hops_table = find_or_create_table($tput_vs_hops_data_per_numflows, $numflows);
            $tput_vs_hops_table->{$numhops} = $throughput;

            my $fwd_vs_hops_table = find_or_create_table($fwd_vs_hops_data_per_numflows, $numflows);
            $fwd_vs_hops_table->{$numhops} = $forwarded;

            my $tput_vs_flows_table = find_or_create_table($tput_vs_flows_data_per_numhops, $numhops);
            $tput_vs_flows_table->{$numflows} = $throughput;

            my $fwd_vs_flows_table = find_or_create_table($fwd_vs_flows_data_per_numhops, $numhops);
            $fwd_vs_flows_table->{$numflows} = $forwarded;

            # Switch back to output level directory
            die "Could not switch directory back to $output_level_dir.\n" unless chdir $output_level_dir;

            # We do a delay before running the next test, to see if we can avoid
            # the puzzling drops in performance that randomly plague some tests.
            print "Finished test with $numhops hops and $numflows flows, sleeping for $sleep_delay seconds.\n";
            sleep $sleep_delay;
        }
    }

    # Make directory for graphs
    die "Could not make graph directory $graph_dir.\n" unless mkdir $graph_dir;
    die "Could not change directory to $graph_dir.\n" unless chdir $graph_dir;

    # Alright, we have all the data. Now we need to plot it.
    plot_tput_vs_hops_data_per_numflows($tput_vs_hops_data_per_numflows);

    plot_fwd_vs_hops_data_per_numflows($fwd_vs_hops_data_per_numflows);

    plot_tput_vs_flows_data_per_numhops($tput_vs_hops_data_per_numflows, @hop_values);

    plot_fwd_vs_flows_data_per_numhops($fwd_vs_hops_data_per_numflows, @hop_values);

     # Done with all tests, back to top level directory.
    die "Could not switch directory back to $top_level_dir.\n" unless chdir $top_level_dir;
    print "Done!\n";
}

# sub find_or_create_table(map, key)
#
# Look inside map with given key to find table associated with key.
# If no table is found, create an empty table and associate with key.
# Return the table.
#
# map: Map containing tables organized by key
# key: Key for looking into map
#
sub find_or_create_table
{
    (my $map, my $key) = @_;
    if (!(exists $map->{$key})) {
        $map->{$key} = {};
    }
    return $map->{$key};
}


# sub do_capacity_test(emulator_name, hosts_file, bw_value, num_hops, num_flows)
#
# Runs the capacity test for the given parameters. 
# emulator_name: hostname for the emulator
# hosts_file: contains one emulator where the name should match the given 
# emulator name, and one or more clients.
# bw_value: bandwidth in kbps per hop
# num_hops: Number of hops each flow must take
# num_flows: Total number of flows
# test_dir: working dir for test output
#
sub do_capacity_test {
    (my $emulator_name, my $hosts_file, my $bw_value, my $num_hops, my $num_flows) = @_;

    # Given per hop bandwidth, # of hops and # of flows, create graph
    die "Could not create topology graph for capacity test!\n"
    unless createtopology($num_flows, $num_hops, $bw_value, $drop_rate, 
               $delay_ms, $queue_len, $graph_filename);
    
    # Create route file from graph
    system("$allpairs_cmd $graph_filename > $route_filename");

    # Assume hosts file already has 1 server and 1 or more clients. Create model.
    system("$mkmodel_cmd $graph_filename $hosts_file > $model_filename");

    # Deploy topology on all systems
    system("$deploy_cmd $model_filename $route_filename");

    # Parse model
    my $parser = new XML::DOM::Parser;
    my $model = $parser->parsefile($model_filename);

    # Keep a map from VN ID to IP
    my $vn_id_to_ip_map = build_vn_id_to_ip_addr_map($model);
    
    # Keep a map from Source VN ID to Destination VN ID
    # Note that any source will start a single iperf client, while any 
    # destination will start a single iperf server. Furthermore, every
    # source is associated with exactly one destination, and vice versa.
    my $vn_src_to_dst_map = build_vn_src_to_dst_map($num_flows, $num_hops);

    # Also maintain a map from src (and dst) to port number 
    # for the iperf client and server processes.
    my $src_to_port_map;
    my $current_port = $base_port;

    # For each destination VN, start a TCP Iperf server.
    for my $src (keys %$vn_src_to_dst_map) {
        my $dst = $vn_src_to_dst_map->{$src};
        my $cmd = "$vnrun_cmd $dst $model_filename iperf -s -p $current_port &";
        $src_to_port_map->{$src} = $current_port;
        $current_port++;

        # Since we're running command with '&', we won't block with system()
        system($cmd);
    }

    # Get # packets that have been forwarded by emulator before start of test.
    # NOTE: We assume no other traffic occurs over emulator while test runs.
    my $num_ip_pkts_fwd_pre_test = get_ip_packets_forwarded_by_host_count($emulator_name, $tmp_file);

    # Start all TCP flows, from each sender to each receiver.
    my @thread_list = ();
    for my $src (keys %$vn_src_to_dst_map) {
        my $dst = $vn_src_to_dst_map->{$src};
        my $src_ip = $vn_id_to_ip_map->{$src};
        my $dst_ip = $vn_id_to_ip_map->{$dst};
        my $port = $src_to_port_map->{$src};

        # Create running thread and add it to list
        my $thread = threads->create(\&start_tcp_flow, $src, $src_ip, $dst, $dst_ip, $port);
        push(@thread_list, $thread);
    }

    # Wait for TCP flows to finish
    for my $thread (@thread_list) {
        $thread->join();
    }

    # Delay before killing all iperfs
    sleep 1;

    # Kill all iperf instances on all systems
    for my $src (keys %$vn_src_to_dst_map) {
        my $dst = $vn_src_to_dst_map->{$src};
        my $src_cmd = "$vnrun_cmd $src $model_filename killall -9 iperf";
        my $dst_cmd = "$vnrun_cmd $dst $model_filename killall -9 iperf";
        system($src_cmd);
        system($dst_cmd);
    }

    # Get # packets that have been forwarded by emulator by end of test.
    my $num_ip_pkts_fwd_post_test = get_ip_packets_forwarded_by_host_count($emulator_name, $tmp_file);
    my $num_ip_pkts_fwd = $num_ip_pkts_fwd_post_test - $num_ip_pkts_fwd_pre_test;

    # Get aggregate throughput for ALL flows in this run.
    my $aggregate_throughput = 0;
    for my $src (keys %$vn_src_to_dst_map) {
        my $dst = $vn_src_to_dst_map->{$src};
        my $filename = "./src_${src}_dst_${dst}_flow";
        $aggregate_throughput += get_tcp_flow_throughput_mbps($filename);
    }

    # Return # packets Forwarded and aggregate throughput for run
    my $retval;
    $retval->{'FORWARDED'} = $num_ip_pkts_fwd;
    $retval->{'THROUGHPUT'} = $aggregate_throughput;
    return $retval;
}

# sub start_tcp_flow
# 
# Starts iperf TCP flow from given src to dst virtual node.
# Note: Blocks the calling thread till client is done. Meant to
# be called in its own thread and waited on for completion.
#
# src_id: source vn id
# src_ip: source vn IP address
# dst_id: destination vn id
# dst_ip: destination vn IP address
# port: destination vn iperf server port number
#
sub start_tcp_flow
{
    (my $src_id, my $src_ip, my $dst_id, my $dst_ip, my $port) = @_;

    print "Starting flow from VN $src_id ($src_ip) to VN $dst_id ($dst_ip) on port $port.\n";

    # Assume output directory exists
    my $cmd = "$vnrun_cmd $src_id $model_filename iperf -c $dst_ip -p $port -fm -t $iperf_runtime > ./src_${src_id}_dst_${dst_id}_flow";
    return system($cmd);
}

# sub build_vn_id_to_ip_addr_map(doc)
#
# Builds a map from VN ID to IP Address and return it.
#
# model: XML document describing model file
#
sub build_vn_id_to_ip_addr_map
{
    (my $model) = @_;

    my $vn_id_to_ip_map;

    my $hosts = $model->getElementsByTagName('host');
    for (my $hostnum = 0; $hostnum < $hosts->getLength(); $hostnum++) {
        my $host = $hosts->item($hostnum);
        my $vns = $host->getElementsByTagName('virtnode');
        for (my $vnnum = 0; $vnnum < $vns->getLength(); $vnnum++) {
            my $vn = $vns->item($vnnum);
            my $ip_addr = $vn->getAttribute('vip');
            my $vn_id = $vn->getAttribute('int_vn');
            die "VN for host $hostnum with index $vnnum has no ID! $vn_id\n" unless $vn_id ne '';
            die "VN with ID $vn_id has no IP Address!\n" unless $ip_addr;
            $vn_id_to_ip_map->{$vn_id} = $ip_addr;
        }
    }

    return $vn_id_to_ip_map;
}

# sub build_vn_src_to_dst_map(num_flows, num_hops)
#
# Builds a map from src VN to dst VN per flow by ID and returns it.
#
# num_flows: # of flows
# num_hops: # of hops
#
sub build_vn_src_to_dst_map
{
    (my $num_flows, my $num_hops) = @_;

    my $vn_src_to_dst_map;

    for my $flow(0 .. $num_flows - 1) {
        my $src_id = $flow * $num_hops;
        my $dst_id = $src_id + $num_hops;
        $vn_src_to_dst_map->{$src_id} = $dst_id;
    }

    return $vn_src_to_dst_map;
}

