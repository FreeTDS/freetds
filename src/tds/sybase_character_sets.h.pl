#!/usr/pkg/bin/perl
## This file is in the public domain.

# get list of character sets we know about
open ALT, "alternative_character_sets.h" or die;
while(<ALT>){
	next unless /^\t[, ] {\s+"(.+?)", "(.+?)"/;
	$alternates{$2} = $1;
}

$alternates{'UTF8'}	= 'UTF-8';
$alternates{'ISO_1'}    = 'ISO-8859-1';
$alternates{'ASCII_8'}  = 'ISO-8859-1';
$alternates{'ISO_1'}    = 'ISO-8859-1';
$alternates{'ISO646'}   = 'ANSI_X3.4-1968';

$alternates{'MAC_CYR'}  = 'MACCYRILLIC';
#alternates{'MAC_EE'}   = '';
$alternates{'MACTURK'}  = 'MACTURKISH';

$alternates{'KOI8'}  = 'KOI8-R';

# get list we produced last time (capture manual edits to file).
open SYB, "sybase_character_sets.h" or warn qq/"sybase_character_sets.h" not found\n/;
while(<SYB>){
	next unless /^\t[, ] {\s+"(.+?)", "(.+?)"/;
	$alternates{$2} = $1;
}

print "/*\n";
print " * This file produced from $0\n";
print ' * $Id: sybase_character_sets.h.pl,v 1.4 2003-12-03 07:13:00 freddy77 Exp $', "\n";
print " */\n";

# look up the canonical name
$comma = ' ';
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
	
	$name = qq/"$name"/;
	if( $iconv_name ) { 	# found, print element
		$iconv_name = qq/"$iconv_name",/;
		printf "\t$comma { %20s %-15s }\n", $iconv_name , $name;
	} else {		# not found, print comment
		$iconv_name = qq/"",/;
		printf "\t /* %20s %-15s */\n", $iconv_name , $name;

		# grep for similar names, as an aid to the to programmer.  
		$name =~ s/[\-\_]+//g;
		print STDERR "$Name:  $name alternative_character_sets.h\n";
	}
	$comma = ',';
}
print  "\t/* stopper row */\n";
printf "\t$comma { %20s %-15s }\n", 'NULL,' , 'NULL';

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
