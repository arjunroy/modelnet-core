#!/usr/bin/perl

#This file create the .model file for modelnet capacity testing

use English;
use XML::DOM;
use IO::File;


my $numflows=$ARGV[0];
my $numhops=$ARGV[1];

my $doc= new XML::DOM::Document;
my $xml_pi=$doc->createXMLDecl('1.0');
my $root=$doc->createElement('model');
my $emulators=$doc->createElement('emulators');
$root->appendChild($emulators);
my $emul_28=$doc->createElement('emul');
$emul_28.setAttribute("hostname","sysnet28.ucsd.edu");
$emul_28.setAttribute("int_idx","0");
$emulators->appendChild($emul_28);
my $host_27=$doc->createElement();
$emul_27.setAttribute("hostname","sysnet27.ucsd.edu");
$host_27->appendChild($emul_28);

