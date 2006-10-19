<?php

// $Id: null.php,v 1.1 2006-10-19 12:08:36 freddy77 Exp $

require_once("pwd.inc");

$conn = mssql_connect($server,$user,$pass) or die("opps");

mssql_query("CREATE TABLE #MyTable (
   myfield VARCHAR(10) NULL
)", $conn) or die("error querying");


mssql_query("INSERT INTO #MyTable VALUES('')
INSERT INTO #MyTable VALUES(NULL)
INSERT INTO #MyTable VALUES(' ')
INSERT INTO #MyTable VALUES('a')", $conn) or die("error querying");

$result = 0;

function test($sql, $expected)
{
	global $conn, $result;

	$res = mssql_query($sql, $conn) or die("query");
	$row = mssql_fetch_assoc($res);
	$s = $row['myfield'];

	if (is_null($s))
		$s = '(NULL)';
	else if ($s == '')
		$s = '(Empty String)';
	else
		$s = "'".str_replace("'", "''", $s)."'";
	echo "$sql -> $s\n";
	if ($s != $expected)
	{
		echo "error!\n";
		$result = 1;
	}
}

test("SELECT top 1 * FROM #MyTable WHERE myfield = ''", "' '");

test("SELECT top 1 * FROM #MyTable WHERE myfield IS NULL", "(NULL)");

test("SELECT top 1 * FROM #MyTable WHERE myfield = ' '", "' '");

test("SELECT top 1 * FROM #MyTable WHERE myfield = 'a'", "'a'");

exit($result);
?>
