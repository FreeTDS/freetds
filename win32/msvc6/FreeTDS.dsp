# Microsoft Developer Studio Project File - Name="FreeTDS" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=FreeTDS - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "FreeTDS.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "FreeTDS.mak" CFG="FreeTDS - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "FreeTDS - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "FreeTDS - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "FreeTDS - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "FREETDS_EXPORTS" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I ".." /I "../../include" /D "HAVE_CONFIG_H" /D "UNIXODBC" /D "_FREETDS_LIBRARY_SOURCE" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "FREETDS_EXPORTS" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x410 /d "NDEBUG"
# ADD RSC /l 0x410 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 ws2_32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386

!ELSEIF  "$(CFG)" == "FreeTDS - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "FREETDS_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I ".." /I "../../include" /D "HAVE_CONFIG_H" /D "UNIXODBC" /D "_FREETDS_LIBRARY_SOURCE" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "FREETDS_EXPORTS" /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x410 /d "_DEBUG"
# ADD RSC /l 0x410 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 ws2_32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept

!ENDIF 

# Begin Target

# Name "FreeTDS - Win32 Release"
# Name "FreeTDS - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\src\odbc\connectparams.c
# End Source File
# Begin Source File

SOURCE=..\..\src\odbc\convert_sql2string.c
# End Source File
# Begin Source File

SOURCE=..\..\src\odbc\convert_tds2sql.c
# End Source File
# Begin Source File

SOURCE=..\..\src\odbc\error.c
# End Source File
# Begin Source File

SOURCE=..\FreeTDS.def
# End Source File
# Begin Source File

SOURCE=..\initnet.c
# End Source File
# Begin Source File

SOURCE=..\..\src\odbc\native.c
# End Source File
# Begin Source File

SOURCE=..\..\src\odbc\odbc.c
# End Source File
# Begin Source File

SOURCE=..\..\src\odbc\odbc_util.c
# End Source File
# Begin Source File

SOURCE=..\..\src\odbc\prepare_query.c
# End Source File
# Begin Source File

SOURCE=..\..\src\odbc\sql2tds.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\config.h
# End Source File
# Begin Source File

SOURCE=..\..\src\odbc\connectparams.h
# End Source File
# Begin Source File

SOURCE=..\..\src\odbc\convert_sql2string.h
# End Source File
# Begin Source File

SOURCE=..\..\src\odbc\convert_tds2sql.h
# End Source File
# Begin Source File

SOURCE=..\..\include\des.h
# End Source File
# Begin Source File

SOURCE=..\freetds_sysconfdir.h
# End Source File
# Begin Source File

SOURCE=..\..\include\md4.h
# End Source File
# Begin Source File

SOURCE=..\..\src\odbc\odbc_util.h
# End Source File
# Begin Source File

SOURCE=..\..\src\odbc\prepare_query.h
# End Source File
# Begin Source File

SOURCE=..\..\include\replacements.h
# End Source File
# Begin Source File

SOURCE=..\..\src\odbc\sql2tds.h
# End Source File
# Begin Source File

SOURCE=..\..\include\tds.h
# End Source File
# Begin Source File

SOURCE=..\..\include\tds_configs.h
# End Source File
# Begin Source File

SOURCE=..\..\include\tds_sysdep_private.h
# End Source File
# Begin Source File

SOURCE=..\tds_sysdep_public.h
# End Source File
# Begin Source File

SOURCE=..\..\src\tds\tds_willconvert.h
# End Source File
# Begin Source File

SOURCE=..\..\include\tdsconvert.h
# End Source File
# Begin Source File

SOURCE=..\..\include\tdsiconv.h
# End Source File
# Begin Source File

SOURCE=..\..\include\tdsodbc.h
# End Source File
# Begin Source File

SOURCE=..\..\include\tdsstring.h
# End Source File
# Begin Source File

SOURCE=..\..\include\tdsver.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# Begin Group "Replacements"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\src\replacements\asprintf.c
# End Source File
# Begin Source File

SOURCE=..\..\src\replacements\strtok_r.c
# End Source File
# Begin Source File

SOURCE=..\..\src\replacements\vasprintf.c
# End Source File
# End Group
# Begin Group "LibTDS"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\src\tds\challenge.c
# End Source File
# Begin Source File

SOURCE=..\..\src\tds\config.c
# End Source File
# Begin Source File

SOURCE=..\..\src\tds\convert.c
# End Source File
# Begin Source File

SOURCE=..\..\src\tds\des.c
# End Source File
# Begin Source File

SOURCE=..\..\src\tds\getmac.c
# End Source File
# Begin Source File

SOURCE=..\..\src\tds\iconv.c
# End Source File
# Begin Source File

SOURCE=..\..\src\tds\locale.c
# End Source File
# Begin Source File

SOURCE=..\..\src\tds\login.c
# End Source File
# Begin Source File

SOURCE=..\..\src\tds\md4.c
# End Source File
# Begin Source File

SOURCE=..\..\src\tds\mem.c
# End Source File
# Begin Source File

SOURCE=..\..\src\tds\numeric.c
# End Source File
# Begin Source File

SOURCE=..\..\src\tds\query.c
# End Source File
# Begin Source File

SOURCE=..\..\src\tds\read.c
# End Source File
# Begin Source File

SOURCE=..\..\src\tds\tdsstring.c
# End Source File
# Begin Source File

SOURCE=..\..\src\tds\threadsafe.c
# End Source File
# Begin Source File

SOURCE=..\..\src\tds\token.c
# End Source File
# Begin Source File

SOURCE=..\..\src\tds\util.c
# End Source File
# Begin Source File

SOURCE=..\..\src\tds\vstrbuild.c
# End Source File
# Begin Source File

SOURCE=..\..\src\tds\write.c
# End Source File
# End Group
# End Target
# End Project
