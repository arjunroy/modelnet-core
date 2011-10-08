#!/usr/bin/perl

#DATAFILE="r1.dat";

open (MYFILE, 'r1.dat');
my $lastnum=0;
my $fileline="";
foreach $line (<MYFILE>) {
	@array=split(" ", $line);
	if ($lastnum != 0) {
		$tmp=(@array[0] - $lastnum)/20;
		$fileline=$fileline.$tmp."\t";
		$lastnum=@array[0];
	} else {
		$lastnum=@array[0];
	}
}
$fileline=$fileline."\t".$ARGV[0];
`echo $fileline >> result.dat`;
close (MYFILE);
