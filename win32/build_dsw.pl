#!/usr/bin/env perl

use strict;

my ($template, $dsp, $name);

mkdir('vc6');

$template = q|# Microsoft Developer Studio Project File - Name="t0002" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=t0002 - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "t0002.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "t0002.mak" CFG="t0002 - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "t0002 - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "t0002 - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "t0002 - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "..\\Release"
# PROP BASE Intermediate_Dir "..\\Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\\Release"
# PROP Intermediate_Dir "..\\Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /D FREETDS_SRCDIR=\\"..\\" /D DBNTWIN32 /YX /FD /c
# ADD BASE RSC /l 0x410 /d "NDEBUG"
# ADD RSC /l 0x410 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib  kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib  kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib ntwdblib.lib /nologo /subsystem:console /machine:I386

!ELSEIF  "$(CFG)" == "t0002 - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "..\\Debug"
# PROP BASE Intermediate_Dir "..\\Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\\Debug"
# PROP Intermediate_Dir "..\\Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /GZ  /c
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /D FREETDS_SRCDIR=\\"..\\" /D DBNTWIN32 /YX /FD /GZ  /c
# ADD BASE RSC /l 0x410 /d "_DEBUG"
# ADD RSC /l 0x410 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib  kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib  kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib ntwdblib.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept

!ENDIF 

# Begin Target

# Name "t0002 - Win32 Release"
# Name "t0002 - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\common.c
# End Source File
# Begin Source File

SOURCE=..\t0002.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\common.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# End Target
# End Project
|;

my ($dsw, $projects, $fn_out);
my ($executables, $link_cmds, $tests);

$fn_out = shift @ARGV;

$projects = '';
foreach $name (@ARGV) {
	$name =~ s/\.exe$//i;
	$projects .= qq|###############################################################################

Project: "$name"=.\\vc6\\$name.dsp - Package Owner=<4>

Package=<5>
{{{
}}}

Package=<4>
{{{
}}}

|;
	$dsp = $template;
	$dsp =~ s/t0002/$name/g;
	$dsp =~ s/\n/\r\n/sg;
	open(FILE, ">", "vc6/$name.dsp") or die("creating file");
	print FILE $dsp;
	close(FILE);

	$executables .= qq| \\\n\t"\$(OUTDIR)\\$name.exe"|;
	$link_cmds .= qq|"\$(OUTDIR)\\$name.exe" : "\$(OUTDIR)" "\$(INTDIR)\\common.obj" "\$(INTDIR)\\$name.obj"
	\$(LINK32) \$(LINK32_FLAGS) "\$(INTDIR)\\common.obj" "\$(INTDIR)\\$name.obj" /pdb:"\$(OUTDIR)\\$name.pdb" /out:"\$(OUTDIR)\\$name.exe"

|;
	$tests .= qq|\n\t"\$(OUTDIR)\\$name.exe"|;
}

$template = qq|Microsoft Developer Studio Workspace File, Format Version 6.00
# WARNING: DO NOT EDIT OR DELETE THIS WORKSPACE FILE!

$projects
###############################################################################

Global:

Package=<5>
{{{
}}}

Package=<3>
{{{
}}}

###############################################################################

|;

open(FILE, ">", $fn_out) or die("creating file");
$template =~ s/\n/\r\n/sg;
print FILE $template;
close(FILE);

$template = qq|!IF "\$(OS)" == "Windows_NT"
NULL=
!ELSE
NULL=nul
!ENDIF

OUTDIR=.\\Release
INTDIR=.\\Release

ALL : $executables

CLEAN :
	-\@erase "\$(INTDIR)\*.obj"
	-\@erase "\$(OUTDIR)\*.exe"

"\$(OUTDIR)" :
	if not exist "\$(OUTDIR)/\$(NULL)" mkdir "\$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MD /W3 /O2 /Ob2 /I "./" /D WIN32 /D NDEBUG /D _CONSOLE /D _MBCS /D FREETDS_SRCDIR=\\"..\\" /D DBNTWIN32 /Fo"\$(INTDIR)\\\\" /Fd"\$(INTDIR)\\\\" /FD /c 

.c{\$(INTDIR)}.obj::
	\$(CPP) \$(CPP_PROJ) \$<

LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib advapi32.lib ws2_32.lib odbc32.lib ntwdblib.lib /nologo /subsystem:console /incremental:no /machine:I386

$link_cmds

CHECK :	$tests
|;

$fn_out =~ s/\.dsw$/.mak/i;

open(FILE, ">", $fn_out) or die("creating file");
$template =~ s/\n/\r\n/sg;
print FILE $template;
close(FILE);
