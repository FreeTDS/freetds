#!/usr/bin/perl

print "/*\n";
$date = localtime;
print " * This file produced from $0 on $date\n";
print ' * $Id: encodings.pl,v 1.6 2003-09-19 06:10:15 freddy77 Exp $', "\n";
print " */\n";

%charsets = ();
open(IN, "<character_sets.h") or die("Error opening");
while(<IN>)
{
	if (/{.*"(.*)".*,\s*([0-9]+)\s*,\s*([0-9]+)\s*}/)
	{
		next if !$1;
		$charsets{$1} = [$2,$3];
	}
}
close(IN);

# from all iconv to canonic
%alternates = ();
open(IN, "<alternative_character_sets.h") or die("Error opening");
while(<IN>)
{
	if (/{\s*"(.+)"\s*,\s*"(.+)"\s*}/)
	{
		$alternates{$2} = $1;
	}
}
close(IN);

# from sybase to canonic
%sybase = ();
open(IN, "<sybase_character_sets.h") or die("Error opening");
while(<IN>)
{
	if (/{\s*"(.+)"\s*,\s*"(.+)"\s*}/)
	{
		$sybase{$2} = $alternates{$1};
	}
}
close(IN);

# give an index to all canonic
%index = ();
$i = 0;
$index{"ISO-8859-1"} = $i++;
$index{"UTF-8"} = $i++;
$index{"UCS-2LE"} = $i++;
$index{"UCS-2BE"} = $i++;
foreach $n (sort grep(!/^(ISO-8859-1|UTF-8|UCS-2LE|UCS-2BE)$/,keys %charsets))
{
	$index{$n} = $i;
	$i++ if $n;
}

# output all charset
print "static const TDS_ENCODING canonic_charsets[] = {\n";
$i=0;
foreach $n (sort { $index{$a} <=> $index{$b} } keys %charsets)
{
	my ($a, $b) = @{$charsets{$n}};
	my $index = ($n)? (sprintf "/* %3d */", $i) : "\t";
	printf "\t{%20s,\t$a, $b},\t%s\n", qq/"$n"/, $index;
	$i++ if $n;
}
print "\t{\"\",\t0, 0}\n";
print "};\n\n";

# output all alias
print "static const CHARACTER_SET_ALIAS iconv_aliases[] = {\n";
foreach $n (sort keys %alternates)
{
	$a = $index{$alternates{$n}};
	next if ("$a" eq "");
	printf "\t{%25s,  %3d },\n", qq/"$n"/, $a;
}
print "\t{NULL,\t0}\n";
print "};\n\n";

# output all sybase
print "static const CHARACTER_SET_ALIAS sybase_aliases[] = {\n";
foreach $n (sort keys %sybase)
{
	$a = $index{$sybase{$n}};
	next if ("$a" eq "");
	printf "\t{%20s, %3d },\n", qq/"$n"/, $a;
	#rint qq|\t{"$n",\t$a},\n|;
}
print "\t{NULL,\t0}\n";
print "};\n\n";
