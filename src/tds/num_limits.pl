#!/usr/bin/perl

#
# $Id: num_limits.pl,v 1.1 2005-04-05 11:41:57 freddy77 Exp $
# Compute limits table to check numeric typs
#

use strict;

sub to_pack($$)
{
	my ($num, $bit) = @_;
	my @digits;

	my ($carry, $n, $i, $pos, $bin);
	my $out = "\x00" x 32;
	$pos = 0;
	$bin = '';
	while($num ne '') {
		@digits = split('', $num);
		$carry = 0;
		for $i (0..$#digits) {
			$n = $carry * 10 + $digits[$i];
			$digits[$i] = int($n / 2);
			$carry = $n & 1;
		}
		$bin = "$carry$bin";
		vec($out, $pos++, 1) = $carry;
		shift @digits if ($digits[0] == 0);
		$num = join('', @digits);
	}
	return reverse unpack($bit == 32 ? 'V' x 8 : 'v' x 16, $out);
}

sub print_all()
{
	my ($bit) = @_;
	my @limits = ();
	my @indexes = ();
	my $i;

	$indexes[0] = 0;
	for $i (0..77) {
		my @packet = &to_pack("1".('0'x$i), $bit);
		$indexes[$i] = $#limits + 1;
		while($packet[0] == 0) {
			shift @packet;
		}
		while($packet[0] != 0) {
			push @limits, shift @packet;
		}
		$indexes[$i+1] = $#limits + 1;
	}

	my $adjust = $bit == 32 ? 4 : 6;
	printf("#define LIMIT_INDEXES_ADJUST %d\n\n", $adjust);
	for $i (0..78) {
		my $idx = $indexes[$i];
		$idx = $idx - $adjust * $i;
		die ('error compacting indexes') if ($idx < -128 || $idx > 127);
		$indexes[$i] = $idx;
	}

	# print all
	print "static const signed char limit_indexes[79]= {\n";
	for $i (0..78) {
		printf("\t%d,\t/* %2d */\n", $indexes[$i], $i);
	}
	print "};\n\n";

	print "static const TDS_WORD limits[]= {\n";
	my $format = sprintf("\t0x%%0%dxu,\t/* %%3d */\n", $bit / 4);
	for $i (0..$#limits) {
		printf($format, $limits[$i], $i);
		die ('limit if zero ??') if ($limits[$i] == 0);
	}
	print "};\n";
}

print "#ifndef HAVE_INT64\n";
&print_all(16);
print "#else\n";
&print_all(32);
print "#endif\n";

