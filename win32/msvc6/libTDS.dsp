# Microsoft Developer Studio Project File - Name="libTDS" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=libTDS - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "libTDS.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "libTDS.mak" CFG="libTDS - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "libTDS - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "libTDS - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "libTDS - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "tds_Release"
# PROP BASE Intermediate_Dir "tds_Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "tds_Release"
# PROP Intermediate_Dir "tds_Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I ".." /I "../../include" /D "HAVE_CONFIG_H" /D "UNIXODBC" /D "_FREETDS_LIBRARY_SOURCE" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "FREETDS_EXPORTS" /D "HAVE_SQLGETPRIVATEPROFILESTRING" /D "NDEBUG" /D "_LIB" /YX /FD /c
# ADD BASE RSC /l 0x410 /d "NDEBUG"
# ADD RSC /l 0x410 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "libTDS - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "tds_Debug"
# PROP BASE Intermediate_Dir "tds_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "tds_Debug"
# PROP Intermediate_Dir "tds_Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I ".." /I "../../include" /D "HAVE_CONFIG_H" /D "UNIXODBC" /D "_FREETDS_LIBRARY_SOURCE" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "FREETDS_EXPORTS" /D "HAVE_SQLGETPRIVATEPROFILESTRING" /D "_DEBUG" /D "_LIB" /YX /FD /GZ /c
# ADD BASE RSC /l 0x410 /d "_DEBUG"
# ADD RSC /l 0x410 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "libTDS - Win32 Release"
# Name "libTDS - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\src\tds\bulk.c
# End Source File
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

SOURCE=..\..\src\tds\data.c
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

SOURCE=..\..\src\tds\md5.c
# End Source File
# Begin Source File

SOURCE=..\..\src\tds\hmac_md5.c
# End Source File
# Begin Source File

SOURCE=..\..\src\tds\mem.c
# End Source File
# Begin Source File

SOURCE=..\..\src\tds\net.c
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

SOURCE=..\..\src\tds\log.c
# End Source File
# Begin Source File

SOURCE=..\..\src\tds\vstrbuild.c
# End Source File
# Begin Source File

SOURCE=..\..\src\tds\write.c
# End Source File
# Begin Source File

SOURCE=..\..\src\tds\sspi.c
# End Source File
# Begin Source File

SOURCE=..\..\src\tds\win_mutex.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\config.h
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

SOURCE=..\..\include\md5.h
# End Source File
# Begin Source File

SOURCE=..\..\include\hmac_md5.h
# End Source File
# Begin Source File

SOURCE=..\..\include\replacements.h
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

SOURCE=..\..\include\tdsstring.h
# End Source File
# Begin Source File

SOURCE=..\..\include\tdsver.h
# End Source File
# End Group
# Begin Group "Replacements"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\src\replacements\asprintf.c
# End Source File
# Begin Source File

SOURCE=.\iconv_replacement.c
# End Source File
# Begin Source File

SOURCE=..\..\src\replacements\strlcpy.c
# End Source File
# Begin Source File

SOURCE=..\..\src\replacements\strlcat.c
# End Source File
# Begin Source File

SOURCE=..\..\src\replacements\strtok_r.c
# End Source File
# Begin Source File

SOURCE=..\..\src\replacements\vasprintf.c
# End Source File
# End Group
# End Target
# End Project
