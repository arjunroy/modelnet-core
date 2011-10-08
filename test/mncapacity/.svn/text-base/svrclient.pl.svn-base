#!/usr/bin/perl

use English;
use XML::DOM;
use IO::File;

my $parser=new XML::DOM::Parser;
my $doc=$parser->parsefile("capacity.hosts");

my $nodes=$doc->getElementsByTagName("emul");
my $node= $nodes->item(0);

$node->setAttribute("hostname", "$ARGV[0]");

$nodes=$doc->getElementsByTagName("host");
$node=$nodes->item(0);
$node->setAttribute("hostname", "$ARGV[1]");
$node=$nodes->item(1);
$node->setAttribute("hostname", "$ARGV[2]");

$doc->printToFile("capacity.hosts");

$doc->dispose;
