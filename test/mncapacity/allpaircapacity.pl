#!/usr/bin/perl
# This file create the route files for modelnet capacity test

use English;
use XML::DOM;
use IO::File;

my $numflows=$ARGV[0];
my $numhops=$ARGV[1];

my $doc= new XML::DOM::Document;
my $xml_pi=$doc->createXMLDecl('1.0');
my $root=$doc->createElement('allpairs');
my $edge=0;

for (my $i=i; $i<$numflows; $i++) {
	my $server=($numhops+1) * $i+$numhops;
	my $client=($numhops+1) * $i;
	print "111\n";
	my $path=$doc->createElement('path');
	$path->setAttribute("int_vndst", "$server");
	$path->setAttribute("int_vnsrc", "$client");
	my $temp=$client;
	my $hops="";
	while ($temp != $server) {
		$hops=$hops.$edge." ";
		$temp=$temp+1;
		$edge=$edge+1;
	}
	print $server."    ".$client."\n";
	$path->setAttribute("hops", "$hops");
	$root->appendChild($path);
	$path=$doc->createElement('path');
	$path->setAttribute("int_vndst", "$client");
	$path->setAttribute("int_vnsrc", "$server");
	my $temp=$server;
	my $hops="";
	while ($temp != $client) {
		$hops=$hops.$edge." ";
		$temp=$temp-1;
		$edge=$edge+1;
	}
	$path->setAttribute("hops", "$hops");
	$root->appendChild($path);
}

$xml_pi->printToFile("capacity.route");
$root->printToFile("capacity.route");
$doc->dispose;
