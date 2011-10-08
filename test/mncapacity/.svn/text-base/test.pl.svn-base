#!/usr/bin/perl

use English;
use XML::DOM;
use IO::File;

my $doc= new XML::DOM::Document;
my $xml_pi=$doc->createXMLDecl('1.0');
my $root=$doc->createElement('root');
my $test=$doc->createElement('haha');
$test->setAttribute("gaga", "1");
$root->appendChild($test);

print $xml_pi->toString;
print $root->toString;

