package CapacityTest::CreateGraph;

use strict;
use warnings;

our $VERSION = '1.00';
use base 'Exporter';

our @EXPORT = qw(createtopology);

use XML::DOM;
use IO::File;

# sub createtopology(numflows, numhops, 
# hop_bw, droprate, delay_ms, queue_len, filename)
# Create 'chain' style topology with all vertices in a non-looping chain
# based on given parameters
#
# numflows: # of flows total
# numhop: # of hops total
# hop_bw: bandwidth of hop in kilobits per second
# drop_rate: % packet drop rate on link
# delay_ms: millisecond delay on link
# queue_len: Virtual buffer length on each hop
# filename: name of output file
#
sub createtopology {
    (my $numflows, my $numhops, 
     my $hop_bw, my $drop_rate, my $delay_ms, my $queue_len,
     my $filename) = @_;

    # Remove old file
    unlink $filename;

    # Create new document. Top level tags are 
    # topology, then vertices and edges
    my $doc = new XML::DOM::Document;
    my $xml_pi = $doc->createXMLDecl('1.0');
    my $root = $doc->createElement('topology');
    my $vertices = $doc->createElement('vertices');
    my $edges = $doc->createElement('edges');
    my $edge_counter = 0;
    my $numvertex = ($numflows * $numhops) + 1;

    # Write out first vertex
    my $vertex = $doc->createElement('vertex');
    $vertex->setAttribute('int_idx', '0');
    $vertex->setAttribute('role', 'virtnode');
    $vertex->setAttribute('int_vn', '0');
    $vertices->appendChild($vertex);

    for my $i (1 .. $numvertex) {
        # Add vertex
        my $vertex = $doc->createElement('vertex');
        $vertex->setAttribute('int_idx', "$i");
        $vertex->setAttribute('role', 'virtnode');
        $vertex->setAttribute('int_vn', "$i");
        $vertices->appendChild($vertex);

        # Add edge connecting from previous vertex to new vertex
        my $forward_edge = $doc->createElement('edge');
        my $previous_vertex_id = $i - 1;
        $forward_edge->setAttribute('int_dst', "$i");
        $forward_edge->setAttribute('int_src', "$previous_vertex_id");
        $forward_edge->setAttribute('int_idx', "$edge_counter");
        $edge_counter++;
        $forward_edge->setAttribute('specs', 'client-client');
        $edges->appendChild($forward_edge);

        # Add edge connecting from new vertex to previous vertex
        my $backward_edge = $doc->createElement('edge');
        $backward_edge->setAttribute('int_dst', "$previous_vertex_id");
        $backward_edge->setAttribute('int_src', "$i");
        $backward_edge->setAttribute('int_idx', "$edge_counter");
        $edge_counter++;
        $backward_edge->setAttribute('specs', 'client-client');
        $edges->appendChild($backward_edge);
    }

    # Add spec for 'client-client' hops
    my $specs = $doc->createElement('specs');
    my $client_client = $doc->createElement('client-client');
    $client_client->setAttribute('dbl_plr', "$drop_rate");
    $client_client->setAttribute('dbl_kbps', "$hop_bw");
    $client_client->setAttribute('int_delayms', "$delay_ms");
    $client_client->setAttribute('int_qlen', "$queue_len");
    $specs->appendChild($client_client);

    # Append children nodes to top level node
    $root->appendChild($vertices);
    $root->appendChild($edges);
    $root->appendChild($specs);

    # Print to file, cleanup and return
    $xml_pi->printToFile($filename);
    $root->printToFile($filename);
    $doc->dispose; 

    return 1;
}

1;









