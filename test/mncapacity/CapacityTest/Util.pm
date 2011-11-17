package CapacityTest::Util;

use strict;
use warnings;

our $VERSION = '1.00';
use base 'Exporter';

our @EXPORT = qw(get_tcp_flow_throughput_mbps get_udp_flow_throughput_mbps get_ip_packets_forwarded_by_host_count);

# Prefix variables come from environment, otherwise we fallback.
my $MODELNET_PREFIX = $ENV{'MODELNET_PREFIX'};
my $GEXEC_PREFIX = $ENV{'GEXEC_PREFIX'};

if (!$MODELNET_PREFIX) {
      $MODELNET_PREFIX = '/usr/local';
}
if (!$GEXEC_PREFIX) {
      $GEXEC_PREFIX = '/usr/local';
}

my $gexec_cmd = "$GEXEC_PREFIX/bin/gexec";

# sub get_udp_flow_throughput(filename)
#
# Finds UDP flow from file containing output of iperf UDP client run.
#
# filename: filename containing output from iperf client
#
sub get_udp_flow_throughput_mbps
{
    (my $filename) = @_;

    my $throughput_mbps = 0;
    my $found = 0;
    open(FH, $filename) or die "Could not open $filename.\n";
    while (<FH>) {
        my $line = $_;
        if ($line =~ m/.*\s+(\d+|\d+\.\d+) Mbits\/sec .+/) {
            $throughput_mbps = $1;
            $found = 1;
            last;
        }
    }
    close(FH);
    print "Could not find throughput from $filename.\n" unless $found;
    return $throughput_mbps;
}


# sub get_tcp_flow_throughput(filename)
#
# Finds TCP flow from file containing output of iperf TCP client run.
#
# filename: filename containing output from iperf client
#
sub get_tcp_flow_throughput_mbps
{
    (my $filename) = @_;

    my $throughput_mbps = 0;
    my $found = 0;
    open(FH, $filename) or die "Could not open $filename.\n";
    while (<FH>) {
        my $line = $_;
        if ($line =~ m/.*\s+(\d+|\d+\.\d+) Mbits\/sec/) {
            $throughput_mbps = $1;
            $found = 1;
            last;
        }
    }
    close(FH);
    print "Could not find throughput from $filename.\n" unless $found;
    return $throughput_mbps;
}

# sub get_ip_packets_forwarded_by_host(host, tmp_file)
#
# Gets IP packets forwarded by specified host. Uses specified tmp_file
# as working space: the file is deleted when the method returns.
#
# host: hostname for the system we are interested in
# tmp_file: filename that we work with.
#
sub get_ip_packets_forwarded_by_host_count
{
    (my $host, my $tmp_file) = @_;

    my $num_ip_forwarded_packets = 0;

    # Run netstat on remote system, get forwarded IP packets
    my $cmd = "GEXEC_SVRS=\"${host}\" $gexec_cmd netstat -sw | grep forwarded > $tmp_file";
    system($cmd);
    
    # Open tmp_file in read only mode
    open(FH, "$tmp_file"); 
    while (<FH>) {
        my $line = $_;
        # Match whitespace, a number, whitespace, and forwarded. Return the number
        if ($line =~ m/\D*(\d+) forwarded/) {
            $num_ip_forwarded_packets = $1;
        }
    }

    # Clean up and return
    close(FH);
    unlink $tmp_file;
    return $num_ip_forwarded_packets;
}

1;

