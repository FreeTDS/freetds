@echo off
REM This shell script installs the FreeTDS ODBC driver
set dest=%windir%\System
IF EXIST %windir%\System32\ODBCCONF.exe SET dest=%windir%\System32
ECHO Installing to %dest%
COPY /Y Debug\FreeTDS.dll %dest%\FreeTDS.dll
CD %dest%
regsvr32 FreeTDS.dll
