package CapacityTest::Graph;

use strict;
use warnings;

our $VERSION = '1.00';
use base 'Exporter';

our @EXPORT = qw(plot_tput_vs_hops_data_per_numflows plot_fwd_vs_hops_data_per_numflows plot_tput_vs_flows_data_per_numhops plot_fwd_vs_flows_data_per_numhops);

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

sub plot_tput_vs_hops_data_per_numflows 
{
    (my $tput_vs_hops_data_per_numflows) = @_;

    # Each graph is throughput vs. hops for a fixed number of flows.
    # Thus, #graphs is equal to #keys in the given hash.
    for my $numflows (keys %$tput_vs_hops_data_per_numflows) {
        my $table = $tput_vs_hops_data_per_numflows->{$numflows};

        my $graph_filename = "tput_vs_hops_${numflows}_flows_graph.ps";
        my $graph_data_filename = "tput_vs_hops_${numflows}_flows_data.txt";
        my $graph_plot_filename = "tput_vs_hops_${numflows}_flows_plot.p";

        die "Could not open file $graph_data_filename.\n" unless open(FH, ">$graph_data_filename");

        print FH "# Hops\tThroughput (mbps)\n";
        for my $numhops (sort {$a <=> $b} keys %$table) {
            my $throughput = $table->{$numhops};
            print FH "$numhops\t$throughput\n";
        }
        close(FH);

        open(PLOTSCRIPT, ">$graph_plot_filename") or die "Error opening gnuplot file $graph_plot_filename.\n";
        print PLOTSCRIPT "set terminal postscript\n";
        print PLOTSCRIPT "set xtic auto\n";
        print PLOTSCRIPT "set ytic auto\n";
        print PLOTSCRIPT "set xrange [0:100]\n";
        print PLOTSCRIPT "set autoscale xmax\n";
        print PLOTSCRIPT "set yrange [0:100]\n";
        print PLOTSCRIPT "set autoscale ymax\n";
        print PLOTSCRIPT "set title \"Throughput vs. #Hops with $numflows TCP flows\"\n";
        print PLOTSCRIPT "set xlabel \"# Hops\"\n";
        print PLOTSCRIPT "set ylabel \"Throughput (Mbps)\"\n";
        print PLOTSCRIPT "set output \"$graph_filename\"\n";
        print PLOTSCRIPT "plot \"$graph_data_filename\" using 1:2 title 'Throughput (Mbps)' with linespoints\n";
        close(PLOTSCRIPT); 

        system("gnuplot $graph_plot_filename");
    }
}

sub plot_fwd_vs_hops_data_per_numflows 
{
    (my $fwd_vs_hops_data_per_numflows) = @_;

    # Each graph is packets forwarded vs. hops for a fixed number of flows.
    # Thus, #graphs is equal to #keys in the given hash.
    for my $numflows (keys %$fwd_vs_hops_data_per_numflows) {
        my $table = $fwd_vs_hops_data_per_numflows->{$numflows};

        my $graph_filename = "fwd_vs_hops_${numflows}_flows_graph.ps";
        my $graph_data_filename = "fwd_vs_hops_${numflows}_flows_data.txt";
        my $graph_plot_filename = "fwd_vs_hops_${numflows}_flows_plot.p";

        die "Could not open file $graph_data_filename.\n" unless open(FH, ">$graph_data_filename");

        print FH "# Hops\tPackets forwarded\n";
        for my $numhops (sort {$a <=> $b} keys %$table) {
            my $forwarded = $table->{$numhops};
            print FH "$numhops\t$forwarded\n";
        }
        close(FH);

        open(PLOTSCRIPT, ">$graph_plot_filename") or die "Error opening gnuplot file $graph_plot_filename.\n";
        print PLOTSCRIPT "set terminal postscript\n";
        print PLOTSCRIPT "set xtic auto\n";
        print PLOTSCRIPT "set ytic auto\n";
        print PLOTSCRIPT "set xrange [0:100]\n";
        print PLOTSCRIPT "set autoscale xmax\n";
        print PLOTSCRIPT "set yrange [0:100]\n";
        print PLOTSCRIPT "set autoscale ymax\n";
        print PLOTSCRIPT "set title \"Packets forwarded vs. #Hops with $numflows TCP flows\"\n";
        print PLOTSCRIPT "set xlabel \"# Hops\"\n";
        print PLOTSCRIPT "set ylabel \"Packets forwarded\"\n";
        print PLOTSCRIPT "set output \"$graph_filename\"\n";
        print PLOTSCRIPT "plot \"$graph_data_filename\" using 1:2 title 'Packets forwarded' with linespoints\n";
        close(PLOTSCRIPT); 

        system("gnuplot $graph_plot_filename");
    }
}

sub plot_tput_vs_flows_data_per_numhops
{
    (my $tput_vs_hops_data_per_numflows, my @hop_values) = @_;

    my $graph_filename = 'tput_vs_flows_multiseries_hops.ps';
    my $graph_data_filename = 'tput_vs_flows_multiseries_hops_data.txt';
    my $graph_plot_filename = 'tput_vs_flows_multiseries_hops_plot.p';

    die "Could not open file $graph_data_filename.\n" unless open(FH, ">$graph_data_filename");
    
    # Print header
    print FH '#Flows ';
    for my $numhops (@hop_values) {
        print FH "${numhops}_hops ";
    }
    print FH "\n";

    # For each value of numflows
    for my $numflows (sort {$a <=> $b} keys %$tput_vs_hops_data_per_numflows) {
        my $table = $tput_vs_hops_data_per_numflows->{$numflows};

        print FH "$numflows\t";

        for my $numhops (@hop_values) {
            my $value = $table->{$numhops};
            print FH "$value\t";
        }
        print FH "\n";
    }

    # Done writing out data
    close(FH);

    open(PLOTSCRIPT, ">$graph_plot_filename") or die "Error opening gnuplot file $graph_plot_filename.\n";
    print PLOTSCRIPT "set terminal postscript\n";
    print PLOTSCRIPT "set xtic auto\n";
    print PLOTSCRIPT "set ytic auto\n";
    print PLOTSCRIPT "set xrange [0:100]\n";
    print PLOTSCRIPT "set autoscale xmax\n";
    print PLOTSCRIPT "set yrange [0:100]\n";
    print PLOTSCRIPT "set autoscale ymax\n";
    print PLOTSCRIPT "set title \"Throughput vs. #Flows\"\n";
    print PLOTSCRIPT "set xlabel \"# Flows\"\n";
    print PLOTSCRIPT "set ylabel \"Throughput (Mbps)\"\n";
    print PLOTSCRIPT "set output \"$graph_filename\"\n";
    print PLOTSCRIPT "plot ";
    
    my $i = 2;
    for my $numhops (@hop_values) {
        if ($i == 2) {
            print PLOTSCRIPT "\"$graph_data_filename\" using 1:$i title '$numhops hops' with linespoints";
        }
        else {
            print PLOTSCRIPT ", \"$graph_data_filename\" using 1:$i title '$numhops hops' with linespoints";
        }
        $i++;
    }

    print PLOTSCRIPT ";\n";
    close(PLOTSCRIPT); 

    system("gnuplot $graph_plot_filename");
}

sub plot_fwd_vs_flows_data_per_numhops
{
    (my $fwd_vs_hops_data_per_numflows, my @hop_values) = @_;

    my $graph_filename = 'fwd_vs_flows_multiseries_hops.ps';
    my $graph_data_filename = 'fwd_vs_flows_multiseries_hops_data.txt';
    my $graph_plot_filename = 'fwd_vs_flows_multiseries_hops_plot.p';

    die "Could not open file $graph_data_filename.\n" unless open(FH, ">$graph_data_filename");
    
    # Print header
    print FH '#Flows ';
    for my $numhops (@hop_values) {
        print FH "${numhops}_hops ";
    }
    print FH "\n";

    # For each value of numflows
    for my $numflows (sort {$a <=> $b} keys %$fwd_vs_hops_data_per_numflows) {
        my $table = $fwd_vs_hops_data_per_numflows->{$numflows};

        print FH "$numflows\t";

        for my $numhops (@hop_values) {
            my $value = $table->{$numhops};
            print FH "$value\t";
        }
        print FH "\n";
    }

    # Done writing out data
    close(FH);

    open(PLOTSCRIPT, ">$graph_plot_filename") or die "Error opening gnuplot file $graph_plot_filename.\n";
    print PLOTSCRIPT "set terminal postscript\n";
    print PLOTSCRIPT "set xtic auto\n";
    print PLOTSCRIPT "set ytic auto\n";
    print PLOTSCRIPT "set xrange [0:100]\n";
    print PLOTSCRIPT "set autoscale xmax\n";
    print PLOTSCRIPT "set yrange [0:100]\n";
    print PLOTSCRIPT "set autoscale ymax\n";
    print PLOTSCRIPT "set title \"Packets forwarded vs. #Flows\"\n";
    print PLOTSCRIPT "set xlabel \"# Flows\"\n";
    print PLOTSCRIPT "set ylabel \"Packets forwarded\"\n";
    print PLOTSCRIPT "set output \"$graph_filename\"\n";
    print PLOTSCRIPT "plot ";
    
    my $i = 2;
    for my $numhops (@hop_values) {
        if ($i == 2) {
            print PLOTSCRIPT "\"$graph_data_filename\" using 1:$i title '$numhops hops' with linespoints";
        }
        else {
            print PLOTSCRIPT ", \"$graph_data_filename\" using 1:$i title '$numhops hops' with linespoints";
        }
        $i++;
    }

    print PLOTSCRIPT ";\n";
    close(PLOTSCRIPT); 

    system("gnuplot $graph_plot_filename");
}

1;
