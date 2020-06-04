#!/usr/bin/perl
## This file is in the public domain.
use File::Basename;

$basename = basename($0);
$srcdir = "$ARGV[0]/";
$gperf = $ARGV[1];

# get list of character sets we know about
$filename = "${srcdir}alternative_character_sets.h";
open ALT, $filename or die qq($basename: could not open "$filename"\n);
while(<ALT>){
	next unless /^\t[, ] \{\s+"(.+?)", "(.+?)"/;
	$alternates{$2} = $1;
}
close ALT;

$alternates{'UTF8'}	= 'UTF-8';
$alternates{'ISO_1'}    = 'ISO-8859-1';
$alternates{'ASCII_8'}  = 'ISO-8859-1';
$alternates{'ISO_1'}    = 'ISO-8859-1';
$alternates{'ISO646'}   = 'ANSI_X3.4-1968';

$alternates{'MAC_CYR'}  = 'MACCYRILLIC';
#alternates{'MAC_EE'}   = '';
$alternates{'MACTURK'}  = 'MACTURKISH';

$alternates{'KOI8'}  = 'KOI8-R';

# look up the canonical name
%sybase = ();
while(<DATA>){
	next if /^#/;
	next if /^\s*$/;
	($name) = split;
	$Name = uc $name;
	$iconv_name = $alternates{$Name};

	if( !$iconv_name ) { # try predictable transformations
		$Name =~ s/ISO8859(\d{1,2})$/ISO-8859-$1/;
		$Name =~ s/ISO(\d{1,2})$/ISO-8859-$1/;
		
		$iconv_name = $alternates{$Name};
	}

	if( !$iconv_name ) { # try crude transformation
		$Name =~ s/[\-\_]+//g;
		$iconv_name = $alternates{$Name};
	}

	if( $iconv_name ) {	# found, save
		$sybase{$name} = $iconv_name;
	} else {	# not found, print comment
		# grep for similar names, as an aid to the to programmer.  
		$name =~ s/[\-\_]+//g;
		print STDERR $Name.":  $name alternative_character_sets.h\n";
	}
}


print "/*\n";
$date = localtime;
print " * This file produced from $0 on $date\n";
print " */\n";

%charsets = ();
$filename = "${srcdir}character_sets.h";
open(IN, "<$filename") or die qq($basename: could not open "$filename"\n);
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
$filename = "${srcdir}alternative_character_sets.h";
open(IN, "<$filename") or die qq($basename: could not open "$filename"\n);
while(<IN>)
{
	if (/{\s*"(.+)"\s*,\s*"(.+)"\s*}/)
	{
		$alternates{$2} = $1;
	}
}
close(IN);

# from sybase to canonic
foreach my $name (keys %sybase)
{
	$sybase{$name} = $alternates{$sybase{$name}};
}

# give an index to all canonic
%index = ();
$i = 0;
# first indexes for mandatory encodings
$index{$_} = $i++ for qw(ISO-8859-1 UTF-8 UCS-2LE UCS-2BE);
delete @charsets{qw(UCS-2 UCS-2-INTERNAL UCS-2-SWAPPED UTF-16 UCS-4 UCS-4-INTERNAL UCS-4-SWAPPED UTF-32)};
foreach $n (sort keys %charsets)
{
	next if exists($index{$n}) || !$n;
	$index{$n} = $i++;
}

print "#ifdef TDS_ICONV_ENCODING_TABLES\n\n";

# output all charset
print "static const TDS_ENCODING canonic_charsets[] = {\n";
$i=0;
foreach $n (sort { $index{$a} <=> $index{$b} } keys %charsets)
{
	my ($a, $b) = @{$charsets{$n}};
	next if !$n;
	printf "\t{%20s,\t$a, $b, %3d},\t/* %3d */\n", qq/"$n"/, $i, $i;
	++$i;
}
die('too much encodings') if $i >= 256;
print "};\n\n";

open(OUT, '>charset_lookup.gperf') or die;
print OUT "
struct charset_alias { short int alias_pos; short int canonic; };
%{
static const struct charset_alias *charset_lookup(register const char *str, register size_t len);
%}
%%
";

# output all alias
print "static const CHARACTER_SET_ALIAS iconv_aliases[] = {\n";
foreach $n (sort keys %alternates)
{
	$a = $index{$alternates{$n}};
	next if ("$a" eq "");
	printf "\t{%25s,  %3d },\n", qq/"$n"/, $a;
	print OUT "$n, $a\n"
}
print "\t{NULL,\t0}\n";
print "};\n\n";

# output all sybase
foreach $n (sort keys %sybase)
{
	$a = $index{$sybase{$n}};
	next if ("$a" eq "");
	if (!defined($alternates{$n})) {
		print OUT "$n, $a\n";
	}
}

print "#endif\n\n";

print OUT "%%
";
close(OUT);

open(OUT, '>charset_lookup.h') or die;
open(IN, '-|', $gperf, '-m', '100', '-C', '-K', 'alias_pos', '-t', 'charset_lookup.gperf',
	'-F', ',-1', '-P','-H','hash_charset','-N','charset_lookup', '-L', 'ANSI-C') or die;
while(<IN>) {
	s/\Q(int)(long)&((struct\E/(int)(size_t)&((struct/;
	s/\Q(int)(size_t)&((struct stringpool_t *)0)->stringpool_str\E(\d+),/(int)offsetof(struct stringpool_t, stringpool_str\1),/;
	s/register unsigned int len/register size_t len/ if m/^charset_lookup/;
	print OUT $_;
}
close(IN);
close(OUT);

# output enumerated charset indexes
print "enum {\n";
$i=0;
foreach $n (sort { $index{$a} <=> $index{$b} } keys %charsets)
{
	$n =~ tr/-a-z/_A-Z/;
	printf "\t%30s =%4d,\n", "TDS_CHARSET_$n", $i++;
}
printf "\t%30s =%4d\n};\n\n", "TDS_NUM_CHARSETS", $i++;

exit 0;
__DATA__
#http://www.sybase.com/detail/1,6904,1016214,00.html
						
ascii_8
big5
big5hk
cp1026
cp1047
cp1140
cp1141
cp1142
cp1143
cp1144
cp1145
cp1146
cp1147
cp1148
cp1149
cp1250
cp1251
cp1252
cp1253
cp1254
cp1255
cp1256
cp1257
cp1258
cp273
cp277
cp278
cp280
cp284
cp285
cp297
cp420
cp424
cp437
cp500
cp5026
cp5026yen
cp5035
cp5035yen
cp737
cp775
cp850
cp852
cp855
cp857
cp858
cp860
cp861
cp862
cp863
cp864
cp865
cp866
cp869
cp870
cp871
cp874
cp874ibm
cp875
cp921
cp923
cp930
cp930yen
cp932
cp932ms
cp933
cp935
cp936
cp937
cp939
cp939yen
cp949
cp950
cp954
deckanji
euccns
eucgb
eucjis
eucksc
greek8
iso10
iso13
iso14
iso15
iso646
iso88592
iso88595
iso88596
iso88597
iso88598
iso88599
iso_1
koi8
mac
mac_cyr
mac_ee
macgrk2
macgreek
macthai
macturk
roman8
roman9
rcsu
sjis
tis620
turkish8
utf8
