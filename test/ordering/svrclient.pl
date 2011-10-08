#!/usr/bin/perl

use English;
use XML::DOM;
use IO::File;

my $parser=new XML::DOM::Parser;
my $doc=$parser->parsefile("order.model");

my $nodes=$doc->getElementsByTagName("emul");
my $node= $nodes->item(0);

$node->setAttribute("hostname", "$ARGV[0]");

$nodes=$doc->getElementsByTagName("host");
$node=$nodes->item(0);
$node->setAttribute("hostname", "$ARGV[1]");

$doc->printToFile("order.model");

$doc->dispose;
