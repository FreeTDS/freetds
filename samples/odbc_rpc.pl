#!/usr/local/bin/perl
# $Id: odbc_rpc.pl,v 1.1 2005-04-05 22:11:44 jklowden Exp $

use DBI;
use Getopt::Std;
use File::Basename;

$program = basename($0);

Getopt::Std::getopts('U:P:D:d:h', \%opts);

my ($dsn, $user, $pass, $database);
$dsn  = "dbi:ODBC:JDBC" unless $opts{D};
$user = 'guest'  unless $opts{U};
$pass = 'sybase' unless $opts{P};

die qq(Syntax: \t$program [-D dsn] [-U username] [-P password] procedure [arg1[, argn]]\n) 
	if( $opts{h} || 0 == @ARGV );

# Connect
my $dbh = DBI->connect($dsn, $user, $pass, {RaiseError => 1, AutoCommit => 1})
	or die "Unable for connect to $dsn $DBI::errstr";

# Construct an odbc placeholder list like (?, ?, ?)
# for any arguments after $ARGV[0]. 
my $placeholders;
if( @ARGV > 1 ) {
	my @placeholders = ('?') x (@ARGV - 1); 
	$placeholders = '(' . join( q(, ),  @placeholders ) . ')';
	printf STDERR qq(%d arguments found for procedure "$ARGV[0]"\n), scalar(@placeholders);
}

# Ready the odbc call
my $sql = "{? = call $ARGV[0] $placeholders}";
my $sth = $dbh->prepare( $sql );

##  To find the symbolic constants for DBI, perldoc DBI recommends:
##  	use DBI;         
##  	foreach (@{ $DBI::EXPORT_TAGS{sql_types} }) {
##             printf "%s=%d\n", $_, &{"DBI::$_"};
##  	}

# Bind the return code as "inout".
my $rc;
print STDERR qq(Binding parameter #1, the return code\n);
$sth->bind_param_inout(1, \$rc, SQL_INTEGER);

# Bind the input parameters (we don't do outputs in this example).
# Placeholder numbers are 1-based; the first "parameter" 
# is the return code, $rc, above.
for( my $i=1; $i < @ARGV; $i++ ) {	
    printf STDERR qq(Binding parameter #%d: "$ARGV[$i]"\n), ($i+1);
    $sth->bind_param( 1 + $i, $ARGV[$i] );
}

print STDERR qq(\nExecuting: "$sth->{Statement}" with parameters '), 
	     join(q(', '), @ARGV[1..$#ARGV]), qq('\n);

# Be prepared
my $sth = $dbh->prepare($sql);

# Execute the SQL and print the (possibly several) results
if($sth->execute) {
	$i = 1;
	while ( $sth->{Active} ) { 
		printf "Result #%d:\n", $i++;
		while(@dat = $sth->fetchrow) {
			print q('), join(q(', '), @dat), qq('\n);
		}
	}
}

$dbh->disconnect();

exit 0;
