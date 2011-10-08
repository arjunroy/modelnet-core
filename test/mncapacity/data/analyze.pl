#!/usr/bin/perl

#DATAFILE="r1.dat";

open (MYFILE, 'r1.dat');
my $lastnum=0;
foreach $line (<MYFILE>) {
	print $line;
	@array=split(" ", $line);
	if ($lastnum != 0) {
		$tmp=@array[0] - $lastnum;
		`echo $tmp >> result.log`;
	} else {
		$lastnum=@array[0];
	}
}
close (MYFILE);
