#!/usr/bin/perl

`rm -f seq.in seq.out`;

# This script read data from proc every constant interval
open(SEQ_IN, ">>seq.in") or die("ERROR");
open(SEQ_OUT, ">>seq.out") or die("ERROR");

my $max=-1;
while (true) 
{
	open(IN, "/proc/order_in") or die("open proc file error\n");
	open(OUT, "/proc/order_in") or die("open proc file error\n");
	# read data from /proc
	my @lines=<IN>;
	if (@lines[0] > $max) {
		$max=@lines[-1];
		print SEQ_IN @lines;
		`sleep 1`;
		close OUT;
		close IN;
	}
		
}

close SEQ_OUT;
close SEQ_IN;
