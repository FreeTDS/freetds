#!/usr/local/bin/perl
#
use DBI;

my $dbh = DBI->connect("dbi:ODBC:JDBC", 'guest', 'sybase', {PrintError => 0});

die "Unable for connect to server $DBI::errstr"
    unless $dbh;

my $rc;
my $sth;

$sth = $dbh->prepare("select \@\@servername");
if($sth->execute) {
    while(@dat = $sth->fetchrow) {
		print "@dat\n";
    }
}
