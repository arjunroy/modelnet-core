#!/usr/bin/perl
# This file adjust the format of .model file
# such that the senders and receivers on seperate machines
# in particular, sysnet26 is used as sender and sysnet27 is used as receiver

use English;
use XML::DOM;
use IO::File;

my $numflows=$ARGV[0];
my $numhops=$ARGV[1];

# Basically sends index = ($numhops+1)*$i
# and receiver's index = ($numhops+1)*$i + $numhops

my $parser=new XML::DOM::Parser;
my $doc=$parser->parsefile("capacity.model");

my $nodes=$doc->getElementsByTagName("host");
my $host26=$nodes->item(1);
my $host27=$nodes->item(0);

my $vn26=$host26->getElementsByTagName("virtnode");
my $vn27=$host27->getElementsByTagName("virtnode");


# adjust host sysnet26 first, sysnet26 is the sender
for (my $i=0; $i<$vn26->getLength; $i++) {
	if ($vn26->item($i)->getAttribute("int_vn") % ($numhops+1)  == $numhops) { # this is a receiver
		print "haha\n";
		my $temp=$vn26->item($i)->getParentNode->removeChild($vn26->item($i));
		$vn27->item(0)->getParentNode->appendChild($temp);
	}
}
# adjust host sysnet27, sysnet27 is the receiver
for (my $i=0; $i<$vn27->getLength; $i++) {
	if($vn27->item($i)->getAttribute("int_vn") % ($numhops+1)  == 0) { # this is a sender
		print "haha\n";
		my $temp=$vn27->item($i)->getParentNode->removeChild($vn27->item($i));
		$vn26->item(0)->getParentNode->appendChild($temp);
	}
}

$doc->printToFile("capacity.model");
$doc->dispose;
