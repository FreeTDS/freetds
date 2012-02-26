#!/usr/bin/perl

use strict;
open(IN, $ARGV[0]) or die;

my @types = split(',', 'SQLHANDLE,SQLHENV,SQLHDBC,SQLHSTMT,SQLHDESC,SQLHWND,SQLSMALLINT,SQLUSMALLINT,SQLINTEGER,SQLSMALLINT *,SQLLEN *,SQLULEN *,SQLINTEGER *,SQLPOINTER');

while(<IN>) {
	chomp;
	while ($_ =~ m/\\$/) {
		$_ =~ s/\\$//;
		$_ .= <IN>;
		chomp;
	}
	s/\s+/ /g;
	s/ $//;
	s/^ //;
	if (/define FUNC NAME\(([^\)]+)\) \((.*)\)$/) {
		my $func = $1;
		my $args = $2;
		my $wide = 0;
		my $params_all = '';
		my $params_w = '';
		my $params_a = '';
		my $pass_all = '';
		my $pass_aw = '';
		my $sep = '';
		$wide = 1 if $args =~ / WIDE ?$/;
		$args =~ s/ WIDE ?$//;
#		print "$1 - $2\n";
		while ($args =~ /(P.*?)\(([^\,)]+),?([^\,)]+)\)/g) {
			my ($type, $a, $b) = ($1,$2,$3);
#			print "--- $1 -- $2 -- $3\n";
			if ($type eq 'P') {
				$a =~ s/ FAR \*$/ */;
				die $a if !grep { $_ eq $a } @types;
				$params_all .= "$sep$a $b";
				$params_a   .= "$sep$a $b";
				$params_w   .= "$sep$a $b";
				$pass_all   .= "$sep$b";
				$pass_aw    .= "$sep$b";
			} elsif ($type eq 'PCHARIN' || $type eq 'PCHAROUT') {
				die $b if $b ne 'SQLSMALLINT' && $b ne 'SQLINTEGER';
				if ($type eq 'PCHARIN') {
					$params_all .= "${sep}ODBC_CHAR * sz$a, $b cb$a";
					$params_a   .= "${sep}SQLCHAR * sz$a, $b cb$a";
					$params_w   .= "${sep}SQLWCHAR * sz$a, $b cb$a";
					$pass_all   .= "${sep}sz$a, cb$a";
					$pass_aw    .= "$sep(ODBC_CHAR*) sz$a, cb$a";
				} else {
					$params_all .= "${sep}ODBC_CHAR * sz$a, $b cb${a}Max, $b FAR* pcb$a";
					$params_a   .= "${sep}SQLCHAR * sz$a, $b cb${a}Max, $b FAR* pcb$a";
					$params_w   .= "${sep}SQLWCHAR * sz$a, $b cb${a}Max, $b FAR* pcb$a";
					$pass_all   .= "${sep}sz$a, cb${a}Max, pcb$a";
					$pass_aw    .= "${sep}(ODBC_CHAR*) sz$a, cb${a}Max, pcb$a";
				}
			} elsif ($type eq 'PCHAR') {
				$params_all .= "${sep}ODBC_CHAR * $a";
				$params_a   .= "${sep}SQLCHAR * $a";
				$params_w   .= "${sep}SQLWCHAR * $a";
				$pass_all   .= "${sep}$a";
				$pass_aw    .= "$sep(ODBC_CHAR*) $a";
			} else {
				die $type;
			}
			$sep = ', ';
		}
		print "#ifdef ENABLE_ODBC_WIDE
static SQLRETURN _$func($params_all, int wide);
SQLRETURN ODBC_API $func($params_a) {
	return _$func($pass_aw, 0);
}
SQLRETURN ODBC_API ${func}W($params_w) {
	return _$func($pass_aw, 1);
}
#else
SQLRETURN ODBC_API $func($params_a) {
	return _$func($pass_all);
}
#endif

";
	}
}
close(IN);

exit 0;

__END__
static SQLRETURN _SQLPrepare (SQLHSTMT hstmt, ODBC_CHAR* szSqlStr, SQLINTEGER cbSqlStr , int wide);
SQLRETURN __attribute__((externally_visible)) SQLPrepare (SQLHSTMT hstmt, SQLCHAR* szSqlStr, SQLINTEGER cbSqlStr ) {
 return _SQLPrepare (hstmt, (ODBC_CHAR*) szSqlStr, cbSqlStr ,0);
}
SQLRETURN __attribute__((externally_visible)) SQLPrepareW (SQLHSTMT hstmt, SQLWCHAR * szSqlStr, SQLINTEGER cbSqlStr ) {
 return _SQLPrepare (hstmt, (ODBC_CHAR*) szSqlStr, cbSqlStr ,1);
}
static SQLRETURN _SQLPrepare (SQLHSTMT hstmt, ODBC_CHAR* szSqlStr, SQLINTEGER cbSqlStr , int wide)

