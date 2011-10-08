#!/usr/bin/perl
# Change the bandwidth attribute under spec tag in  bwtest.model file

use English;
use XML::DOM;
use IO::File;

my $parser=new XML::DOM::Parser;
my $doc=$parser->parsefile("order.model");

my $nodes=$doc->getElementsByTagName("client-client");
my $node= $nodes->item(0);

$node->setAttribute("dbl_kbps", "$ARGV[0]");
$doc->printToFile("order.model");

$doc->dispose;




