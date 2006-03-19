#!/usr/bin/perl

$magic = '@!@!@!@!@!@!@!@!@!@!@!@!@!@!@!@';
$result = -1;
$num = '000';

sub data($) {
	my $data = shift;
	if ($started) {
		print OUT "$data\n";
	}
}

sub start_html($$) {
	my ($f, $title) = @_;

	print $f qq|<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN" "http://www.w3.org/TR/html4/loose.dtd">
<html>
<head>
<title>$title</title>
<style type="text/css">
.error { background-color: red; color: white }
.info  { color: blue }
</style>
</head>
<body>
<h1>$title</h1>
<p><a href="index.html">Main</a></p>
<table border="1">
$info
</table>
<br />
|;
}

sub end_html($) {
	my $f = shift;
	print $f qq|<p><a href="index.html">Main</a></p>
</body>
</html>
|;
}

sub out_header($) {
	$_ = shift;
        s,  +,</th><th>,g;
	s,^,<table border="1"><tr><th>,;
	s,$,</th></tr>,;
	print OUT "$_\n";
}

sub out_footer() {
	print OUT "</table><br />";
}

sub out_row($) {
	$_ = shift;
	s,  +,</td><td>,g;
	s,^,<tr><td>,;
	s,$,</td></tr>,;
	s,<td>([^<]*):-\)</td>,<td><font color="green">\1</font></td>,g;
	s,<td>([^<]*):-\(</td>,<td bgcolor="red">\1</td>,g;
	s,<td>([^<]*):\(</td>,<td bgcolor="yellow">\1</td>,g;
	print OUT "$_\n";
}


sub parse() {
	# add file to list to make index later
	push @files, {
		num => $num,
		name => $name,
		fn => $fn,
		result => $result
	};
}

while (<>) {
	s/\r?\n$//;
	if (/(.*)$magic- ([^ ]+) (.+) -$magic(.*)/) {
		$last = $4;
		data($1) if ($1 ne '');
#		print "$2 - $3\n";
		if ($2 eq 'START') {
			++$num;
			$name = $3;
			$started = 1;
			open(OUT, ">test$num.txt") or die('opening temp file');
		} elsif ($2 eq 'END') {
			close(OUT);
			parse();
			# reset state
			$started = 0;
			$result = -1;
			$fn = '';
			$name = '';
		} elsif ($2 eq 'FILE') {
			$fn = $3;
		} elsif ($2 eq 'RESULT') {
			$result = $3;
		} elsif ($2 eq 'INFO') {
			$title = $msg = '';
			if ($3 =~ /^HOSTNAME (.*)/) {
				$title = 'Hostname';
				$msg = $1;
			} elsif ($3 =~ /^GCC (.*)/) {
				$title = 'gcc version';
				$msg = $1;
			} elsif ($3 =~ /^UNAME (.*)/) {
				$title = 'uname -a';
				$msg = $1;
			} elsif ($3 =~ /^DATE (.*)/) {
				$title = 'date';
				$msg = $1;
			}
			if ($title) {
				$msg =~ s,&,&amp;,g;
				$msg =~ s,<,&lt;,g;
				$msg =~ s,>,&gt;,g;
				$info .= "<tr><th>$title</th><td>$msg</td></tr>\n";
			}
		} else {
			if ($3 !~ /compile/ && ($2 ne 'start' || $2 ne 'end')) {
				die("invalid command $2");
			}
		}
		data($last) if ($last ne '');
	} else {
		data($_);
	}
}

open(OUT, ">index.html") or die('creating index');
start_html(OUT, 'Test output');
out_header('Test  Success  Warnings  Log');
foreach $i (@files) {
#	print $$i{'result'};
	$result = $i->{result};
	$name = $i->{name};
	$fn = $i->{fn};
	$num = $i->{num};

	$title = $name;
	$title = 'make tests' if ($name eq 'maketest');
	if ($name =~ /^\.\// && $fn =~ /\/freetds-0/) {
		$title = $fn;
		$title =~ s!.*/freetds-0[a-z0-9RC\.]+/!!;
	}

	# make html from txt
	open(HTML, ">test$num.html") or die('opening html file');
	start_html(HTML, $title);

	open(IN, "<test$num.txt") or die('opening txt file');
	@a = <IN>;
	close(IN);

	$_ = join("", @a);
	$warn = 'no :-)';
	$warn = 'yes :-(' if (/^\+?2:/m);
	s,&,&amp;,g; s,<,&lt;,g; s,>,&gt;,g;
	s,\n\+2:([^\n]*),<span class="error">\1</span>,sg;
	s,\n\+3:([^\n]*),<span class="info">\1</span>,sg;
	s,^2:(.*)$,<span class="error">\1</span>,mg;
	s,^3:(.*)$,<span class="info">\1</span>,mg;
	s,^1:,,mg;
	print HTML "<pre>$_</pre>";

	end_html(HTML);
	close(HTML);

	$succ = $result == 0 ? 'yes :-)' : 'no :-(';
	$warn = 'ignored' if ($result != 0);
	out_row("$title  $succ  $warn  <a href=\"test$num.html\">log</a>");
}
out_footer();
end_html(OUT);
close(OUT);
