#!/usr/bin/perl
## This file is in the public domain.
use strict;
use File::Basename;

my $basename = basename($0);
my $srcdir = "$ARGV[0]/";

# check list of attributes is alphabetically sorted
my $filename = "${srcdir}odbc.h";
my $prev = '';
open ODBC, $filename or die qq($basename: could not open "$filename"\n);
while(<ODBC>){
	next unless /^\sODBC_PARAM\((.*?)\)/;
	my $attr = $1;
	die "$attr following $prev" if lc($attr) le lc($prev);
	$prev = $attr;
}
close ODBC;
