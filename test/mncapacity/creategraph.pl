#!/usr/bin/perl
# Change the bandwidth attribute under spec tag in  bwtest.model file
# The first parameter is the number of flows
# The second parameter is the number of hops for each flows
# The bandwidith for each hop is constantly 50mbps

use English;
use XML::DOM;
use IO::File;

my $numflows=$ARGV[0];
my $numhops=$ARGV[1];
my $hopbw= sprintf("%d",1000000 / ($numflows * $numhops));


#$parser=new XML::DOM::Parser;
#$$doc=$parser->parsefile("capacity.graph");

#We are going to write a new xml file
my $doc= new XML::DOM::Document;
my $xml_pi=$doc->createXMLDecl('1.0');
my $root=$doc->createElement('topology');
my $vertice=$doc->createElement('vertices');
my $edges=$doc->createElement('edges');
my $edge_counter=0;
my $numvertex=$numflows* ($numhops+1);

# write the first vertex
my $vertex=$doc->createElement('vertex');
$vertex->setAttribute("int_idx", "0");
$vertex->setAttribute("role", "virtnode");
$vertex->setAttribute("int_vn", "0");
$vertice->appendChild($vertex);

print "haha\n";
my $edge_counter=0;

for (my $i=1;$i<$numvertex;$i++) {
	my $vertex1=$doc->createElement('vertex');
	$vertex1->setAttribute("int_idx", "$i");
	$vertex1->setAttribute("role", "virtnode");
	$vertex1->setAttribute("int_vn", "$i");
	$vertice->appendChild($vertex1);
	# build the bi-directional edge
	my $edge=$doc->createElement('edge');
	my $temp=$i-1;
	$edge->setAttribute("int_dst", "$i");
	$edge->setAttribute("int_src", "$temp");
	$edge->setAttribute("int_idx", "$edge_counter");
	$edge_counter=$edge_counter+1;
	$edge->setAttribute("specs", "client-client");
	$edges->appendChild($edge);
	my $edge=$doc->createElement('edge');
	$edge->setAttribute("int_dst", "$temp");
	$edge->setAttribute("int_src", "$i");
	$edge->setAttribute("int_idx", "$edge_counter");
	$edge_counter=$edge_counter+1;
	$edge->setAttribute("specs", "client-client");
	$edges->appendChild($edge);
}

my $specs=$doc->createElement('specs');
$client_client=$doc->createElement('client-client');
$client_client->setAttribute("dbl_plr", "0");
$client_client->setAttribute("dbl_kbps", "50000");
$client_client->setAttribute("int_delayms", "1");
$client_client->setAttribute("int_qlen", "1000");
$specs->appendChild($client_client);

$root->appendChild($vertice);
$root->appendChild($edges);
$root->appendChild($specs);


$xml_pi->printToFile("capacity.graph");
$root->printToFile("capacity.graph");

$doc->dispose;



