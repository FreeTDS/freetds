#!/usr/local/bin/perl
# $Id: odbc_rpc.pl,v 1.2 2005-04-05 22:48:35 jklowden Exp $
#
# Contributed by James K. Lowden and is hereby placed in 
# the public domain.  No rights reserved.  
#
# This program demonstrates calling the ODBC "dynamic" functions, using
# placeholders and prepared statements.  
#
# By default, arguments are bound to type SQL_VARCHAR.  If the stored procedure 
# uses other types, they may be specified in the form :TYPE:data, where TYPE is one
# of the DBI sybolic constants.  If your data happen to begin with a colon, 
# prefix the string with ':SQL_VARCHAR:'.  
#
# Example: a datetime parameter:  ':SQL_DATETIME:2005-04-01 16:46:00' 
#
# To find the symbolic constants for DBI, perldoc DBI recommends:
#  	use DBI;         
#  	foreach (@{ $DBI::EXPORT_TAGS{sql_types} }) {
#             printf "%s=%d\n", $_, &{"DBI::$_"};
#  	}
# 

use DBI qw(:sql_types);
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

# Bind the return code as "inout".
my $rc;
print STDERR qq(Binding parameter #1, the return code\n);
$sth->bind_param_inout(1, \$rc, SQL_INTEGER);

# Bind the input parameters (we don't do outputs in this example).
# Placeholder numbers are 1-based; the first "parameter" 
# is the return code, $rc, above.
for( my $i=1; $i < @ARGV; $i++ ) {
    my $type = SQL_VARCHAR;
    my $typename = 'SQL_VARCHAR';
    my $data = $ARGV[$i];
    if( $data =~ /^:([[:upper:]_]+):(.+)/ ) { # parse out the datatype, if any
	$typename = $1;
	$data = $2;
        $type = eval($typename);
    }
    printf STDERR qq(Binding parameter #%d (type %s): "$data"\n), ($i+1), $typename;
    $sth->bind_param( 1 + $i, $data, $type );
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
