#!/usr/bin/perl

use strict;

sub readLine()
{
	my $line;
	while ($line = <IN>) {
		chomp $line;
		# remove comments
		$line =~ s/#.*//;
		# removed unused spaces at the end
		$line =~ s/\s+$//;
		# if not empty return it
		return $line if $line;
	}
	return $line;
}

sub fixField($)
{
	shift;
	return $_ if substr($_,0,1) ne '"';
	$_ = substr($_, 1, length($_) - 2);
	$_ =~ s/""/"/g;
	return $_;
}

sub splitRow($)
{
	my $row = "$_[0];";
	return map { fixField($_) } ($row =~ /\G("(?:[^"]|"")*"|[^";]*);/sg);
}

# read header
open(IN, '<', $ARGV[0]) or die $ARGV[0];
my $hdr = lc(readLine());
$hdr =~ s/ /_/g;
my @fields = splitRow($hdr);

# read all files
my %types;
my $line;
while ($line = readLine()) {
	my %type;
	@type{@fields} = splitRow($line);
	next if !$type{'name'};
	$types{$type{'name'}} = \%type;
}
close(IN);

# read types values from tdsproto.h
open(IN, '<', $ARGV[1]) or die $ARGV[1];
while (<IN>) {
	if (/\s+(X?SYB[A-Z0-9]+)\s+=\s+([1-9]\d+)/) {
		my ($name, $val) = ($1, $2);
		next if !exists($types{$name});
		die "out of range" if $val <= 0 || $val >= 256;
		$types{$name}->{value} = $val;
	}

}
close(IN);
foreach (keys %types) {
	die "no value for type $_" if !$types{$_}->{value};
}

print qq|/*
 * This file produced from $0
 */

|;

sub unique($)
{
	return keys %{{ map { $_ => 1 } @_ }};
}

sub switchValues()
{
	my ($indent, $column, @list) = @_;
	foreach my $value (sort { $a <=> $b } &unique(map { $_->{$column} } @list)) {
		print $indent.qq|case $_:\n| for (sort map { $_->{'name'} } grep { $_->{$column} eq $value } @list);
		print $indent.qq|	return $value;\n|;
	}
}

# generate tds_get_size_by_type function
print q|/**
 * Return the number of bytes needed by specified type.
 */
int
tds_get_size_by_type(int servertype)
{
	switch (servertype) {
|;
my @list = grep { $_->{'size'} != -1 } values %types;
&switchValues("\t", 'size', @list);
print q|	default:
		return -1;
	}
}

|;

# generate tds_get_varint_size
print q|/**
 * tds_get_varint_size() returns the size of a variable length integer
 * returned in a TDS 7.0 result string
 */
int
tds_get_varint_size(TDSCONNECTION * conn, int datatype)
{
	switch (datatype) {
|;
@list = grep { $_->{'varint'} != 1 && $_->{'varint'} ne '??' && uc($_->{'vendor'}) ne 'MS' && uc($_->{'vendor'}) ne 'SYB' } values %types;
&switchValues("\t", 'varint', @list);
print q|	}

	if (IS_TDS7_PLUS(conn)) {
		switch (datatype) {
|;
@list = grep { $_->{'varint'} != 1 && $_->{'varint'} ne '??' && uc($_->{'vendor'}) eq 'MS' } values %types;
&switchValues("\t\t", 'varint', @list);
print q|		}
	} else if (IS_TDS50(conn)) {
		switch (datatype) {
|;
@list = grep { $_->{'varint'} != 1 && $_->{'varint'} ne '??' && uc($_->{'vendor'}) eq 'SYB' } values %types;
&switchValues("\t\t", 'varint', @list);
print q|		}
	}
	return 1;
}

|;

#generate

print q|/**
 * Return type suitable for conversions (convert all nullable types to fixed type)
 * @param srctype type to convert
 * @param colsize size of type
 * @result type for conversion
 */
int
tds_get_conversion_type(int srctype, int colsize)
{
	switch (srctype) {
|;
# extract all types that have nullable representation
# exclude SYB5INT8 cause it collide with SYBINT8
@list = grep { $_->{'nullable_type'} ne "0" && $_->{name} ne 'SYB5INT8' } values %types;
foreach my $type (@list)
{
	die("$type->{name} should be not nullable") if $type->{nullable};
	die("$type->{name} has invalid nullable type") if !exists($types{$type->{nullable_type}});
}
foreach my $type (sort &unique(map { $_->{nullable_type} } @list)) {
	my @list2 = grep { $_->{nullable_type} eq $type } @list;
	print qq|	case $type:\n|;
	if ($#list2 == 0) {
		print qq|		return $list2[0]->{name};\n|;
	} else {
		print qq|		switch (colsize) {\n|;
		foreach my $item (sort { $b->{size} <=> $a->{size} } @list2) {
			print qq|		case $item->{size}:
			return $item->{name};\n|;
		}
		print qq|		}
		break;\n|;
	}
}
print q|	case SYB5INT8:
		return SYBINT8;
	}
	return srctype;
}

|;

# generate flags
my @bynum;
foreach my $type (sort { $$a{value} <=> $$b{value} } values %types) {
	my %t = %{$type};
	my $num = $t{value};
	if ($t{vendor} !~ /^SYB/) {
		die if $bynum[$num];
		$bynum[$num] = $type;
	}
	if ($t{vendor} !~ /^MS/) {
		die if $bynum[$num+256];
		$bynum[$num+256] = $type;
	}
#	die "wrong fields" if $t{nullable} && $t{fixed};
	die "wrong fields" if $t{variable} && $t{fixed};
	my @f;
	foreach my $n (qw(nullable fixed variable numeric collate unicode ascii)) {
		push @f, uc("TDS_TYPEFLAG_$n") if $t{$n} eq '1';
	}
	my $f = join("|", @f);
	$f = '0' if !$f;
	$type->{flags} = $f;
}

sub name($) {
	return @_[0] ? @_[0] : 'empty';
}

# fill empty array
foreach my $n (0..511) {
	next if ($bynum[$n]);
	$bynum[$n] = { name => '', flags => "TDS_TYPEFLAG_INVALID" };
}

sub collapse($) {
	my $flag = shift;
	# try to collapse flag
	foreach my $n1 (0..511) {
		my $n2 = ($n1 + 256) % 512;
		if ($bynum[$n1]->{name} && !$bynum[$n2]->{name} && $bynum[$n1]->{flags} =~ /$flag/) {
			$bynum[$n2]->{flags} .= "|$flag";
		}
	}
	# check flag are the same on both sized
	foreach my $n1 (0..255) {
		my $n2 = ($n1 + 256) % 512;
		my $fix1 = !!($bynum[$n1]->{flags} =~ /$flag/);
		my $fix2 = !!($bynum[$n2]->{flags} =~ /$flag/);
		next if grep { $bynum[$n2]->{name} eq $_ } qw(SYBBOUNDARY SYBSENSITIVITY);
		die "not collapsing $bynum[$n1]->{name} (Microsoft) and $bynum[$n2]->{name} (Sybase) for $flag" if $fix1 != $fix2;
	}
}

collapse('TDS_TYPEFLAG_FIXED');
collapse('TDS_TYPEFLAG_NULLABLE');
collapse('TDS_TYPEFLAG_NUMERIC');
collapse('TDS_TYPEFLAG_VARIABLE');
collapse('TDS_TYPEFLAG_UNICODE');
collapse('TDS_TYPEFLAG_ASCII');

# output MS flags
print q|const unsigned char tds_type_flags_ms[256] = {
|;
foreach my $n (0..255) {
	my %t = %{$bynum[$n]};
	my $f = $t{flags};
	my $name = name($t{name});
	printf "\t/* %3d %-20s */\t%s,\n", $n, $name, $f;
}
print q|};
|;

# output SYB flags
print q|
#if 0
const unsigned char tds_type_flags_syb[256] = {
|;
foreach my $n (0..255) {
	my %t = %{$bynum[$n+256]};
	my $f = $t{flags};
	my $name = name($t{name});
	printf "\t/* %3d %-20s */\t%s,\n", $n, $name, $f;
}
print q|};
|;


print q|
const char *const tds_type_names[256] = {
|;
foreach my $n (0..255) {
	my $name = '';
	if ($bynum[$n]->{name}) {
		my %t = %{$bynum[$n]};
		$name = $t{name};
	}
	if ($bynum[$n+256]->{name} && $bynum[$n]->{name} ne $bynum[$n+256]->{name}) {
		my %t = %{$bynum[$n+256]};
		$name .= ' or ' if $name;
		$name .= $t{name};
	}
	printf "\t/* %3d */\t\"%s\",\n", $n, $name;
}
print q|};
#endif
|;
