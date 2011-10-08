#!/usr/bin/perl
# This script compares the benchmark result from the standard one
# and print out the result which is far beyond the standard.
# The comparison result is printed to 'diff.dat'


`rm -f diff.dat`;

open (STD_DATA, 'std_result.dat') or die ("error open the file");
open (DATA, 'result.dat') or die ("error open the file");

my @std_data=<STD_DATA>;
my @data=<DATA>;


# There are totally seven rows and six columns and we are going to exame
# them one by one

my $row= scalar @std_data;
for ($i=0; $i < $row; $i++) {
	my @std_line=split(' ', @std_data[$i]);
	my @line=split(' ', @data[$i]);
	my $col= scalar @std_line;
	for ($j=0; $j < $col; $j++) {
		my $diff=abs (@line[$j] - @std_line[$j]);
		my $percentage= $diff / @std_line[$j];
		print $percentage."\n";
		if ($percentage >= 0.2) {
			#if the test value if 20% off by the standard value, report it
			if ($j == 0) {
				$numhops= 1;
			} elsif ($j ==1) {
				$numhops= 2;
			} elsif ($j ==2) {
				$numhops= 4;
			} elsif ($j ==3) {
				$numhops= 8;
			} else {
				$numhops= 12;
			}
			$numflows=@line[5];
			`echo ==================================== >> diff.dat`;
			`echo The test result @line[$j] is far beyond the standard result @std_line[$j], while sending $numflows TCP flows along the path with $numhops pipes. >> diff.dat`;
		}
	}
}
