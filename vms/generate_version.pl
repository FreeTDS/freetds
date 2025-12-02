#!/usr/bin/env perl

use strict;
use warnings;

my $input_file     = 'configure.ac';
my $version_in     = 'include/freetds/version.h.in';
my $version_out    = 'include/freetds/version.h';

# Extract version from AC_INIT in configure.ac
open my $in, '<', $input_file or die "Cannot open $input_file: $!";
my $version;
while (<$in>) {
    if (/AC_INIT\s*\(\s*FreeTDS\s*,\s*([^\)]+)\)/) {
        $version = $1;
        last;
    }
}
close $in;
die "AC_INIT line not found" unless defined $version;

# Replace dev suffix with dev.CURRENTDATE
my @t = localtime();
my $build_number = sprintf("%04d%02d%02d", $t[5]+1900, $t[4]+1, $t[3]);
$version =~ s/\.dev\..*/.dev.$build_number/;
my $output_version = $version;

# Prepare values for usages that require numbers only
# For this purpose, translate 1.6.dev to 1.5.9999
# Currently this appears only to be the ODBC driver version report,
# which expects subversion to have a maximum of 4 digits.
my $ver = $version;
$ver =~ s/dev.*$/9999/;      # Replace dev with 9999
$ver =~ s/rc\d*/9999/i;      # Replace rc with 9999
my ($major, $minor, $subversion) = split(/\./, $ver);
$major      //= 0;
$minor      //= 0;
$subversion //= 0;

if ($subversion !~ /^\d{1,4}$/) {
    die "Invalid SUBVERSION format: $subversion";
}

if ($subversion eq '9999') {
    $minor = $minor - 1;
}

# Substitute tokens in version.h.in
open my $vin, '<', $version_in or die "Cannot open $version_in: $!";
my @lines = <$vin>;
close $vin;

foreach my $line (@lines) {
    $line =~ s/\@PACKAGE\@/FreeTDS/g;
    $line =~ s/\@VERSION\@/$output_version/g;
    $line =~ s/\@MAJOR\@/$major/g;
    $line =~ s/\@MINOR\@/$minor/g;
    $line =~ s/\@SUBVERSION\@/$subversion/g;
    $line =~ s/\@BUILD_NUMBER\@/$build_number/g;
}

open my $vout, '>', $version_out or die "Cannot write $version_out: $!";
print $vout @lines;
close $vout;

print "Generated $version_out successfully.\n";
print "VERSION=$output_version MAJOR=$major MINOR=$minor SUBVERSION=$subversion BUILD_NUMBER=$build_number\n";

