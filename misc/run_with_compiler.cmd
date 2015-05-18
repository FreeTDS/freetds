:: Copied and adapted to build FreeTDS from
:: https://raw.githubusercontent.com/pypa/python-packaging-user-guide/master/source/code/run_with_compiler.cmd
:: (License: CC0 1.0 Universal: http://creativecommons.org/publicdomain/zero/1.0/)
:: by Olivier Grisel.
::
:: Note: this script needs to be run with the /E:ON and /V:ON flags for the
:: cmd interpreter, at least for (SDK v7.0)
::
@ECHO OFF

SET COMMAND_TO_RUN=%*
SET WIN_SDK_ROOT=C:\Program Files\Microsoft SDKs\Windows

IF "%VS_VERSION%" == "2008" (
    SET WINDOWS_SDK_VERSION="v7.0"
) ELSE IF "%VS_VERSION%" == "2010" (
    SET WINDOWS_SDK_VERSION="v7.1"
) ELSE (
    ECHO Unsupported Visual Studio version: "%VS_VERSION%"
    EXIT 1
)

IF "%WIDTH%"=="64" (
    ECHO Configuring Windows SDK %WINDOWS_SDK_VERSION% on a 64 bit architecture
    "%WIN_SDK_ROOT%\%WINDOWS_SDK_VERSION%\Setup\WindowsSdkVer.exe" -q -version:%WINDOWS_SDK_VERSION%
    "%WIN_SDK_ROOT%\%WINDOWS_SDK_VERSION%\Bin\SetEnv.cmd" /x64 /release
    ECHO Executing: %COMMAND_TO_RUN%
    call %COMMAND_TO_RUN% || EXIT 1
) ELSE (
    IF "%VS_VERSION%" == "2008" (
        ECHO Using MSVC 2008 build environment for 32 bit architecture
        "C:\Program Files (x86)\Microsoft Visual Studio 9.0\VC\vcvarsall.bat" x86
    ) ELSE IF "%VS_VERSION%" == "2010" (
        ECHO Configuring Windows SDK %WINDOWS_SDK_VERSION% on a 32 bit architecture
        "%WIN_SDK_ROOT%\%WINDOWS_SDK_VERSION%\Setup\WindowsSdkVer.exe" -q -version:%WINDOWS_SDK_VERSION%
        "%WIN_SDK_ROOT%\%WINDOWS_SDK_VERSION%\Bin\SetEnv.cmd" /x86 /release
    )
    ECHO Executing: %COMMAND_TO_RUN%
    call %COMMAND_TO_RUN% || EXIT 1
)
