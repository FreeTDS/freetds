# Build a Windows Installer package from an installed FreeTDS tree.
# Requires WiX Toolset v3 (WIX environment variable), version 3.14 for arm64,
# and a Visual Studio environment to find the runtime libraries to distribute.
# InstallDir must already contain any other library to distribute, like OpenSSL.
# With -Test the package is also installed and removed, this requires
# administrator rights, changes the system and needs a system able to run Arch.
param(
    [Parameter(Mandatory=$true)][string]$SourceDir,
    [Parameter(Mandatory=$true)][string]$InstallDir,
    [Parameter(Mandatory=$true)][ValidateSet('x86','x64','arm','arm64')][string]$Arch,
    [Parameter(Mandatory=$true)][string]$OutFile,
    [switch]$Test
)

$ErrorActionPreference = 'Stop'

function Run {
    param([string]$Exe, [string[]]$Arguments)
    & $Exe @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Exe failed with code $LASTEXITCODE" }
}

if (-not $env:WIX) { throw 'WIX environment variable not set, is WiX Toolset v3 installed?' }
$wixBin = Join-Path $env:WIX 'bin'

$SourceDir = (Resolve-Path $SourceDir).Path
$InstallDir = (Resolve-Path $InstallDir).Path
$OutFile = [IO.Path]::GetFullPath((Join-Path (Get-Location).Path $OutFile))
$work = Join-Path (Split-Path $OutFile) 'msi-work'
New-Item -ItemType Directory -Force -Path $work | Out-Null

# Windows Installer versions have only 3 significant numbers
$versionH = Get-Content (Join-Path $InstallDir 'include\freetds\version.h') -Raw
$parts = 'MAJOR', 'MINOR', 'SUBVERSION' | ForEach-Object {
    if ($versionH -notmatch "TDS_VERSION_$_\s+(\d+)") { throw "TDS_VERSION_$_ not found" }
    $Matches[1]
}
$version = $parts -join '.'

# Use the runtime library installed with the program as may not be available
# on the system and the ODBC driver could not be registered without it.
if (-not $env:VCToolsRedistDir) { throw 'VCToolsRedistDir not set, run inside a Visual Studio environment' }
$crt = Get-ChildItem (Join-Path $env:VCToolsRedistDir "$Arch\Microsoft.VC*.CRT\vcruntime*.dll")
if (-not $crt) { throw "Runtime libraries not found in $env:VCToolsRedistDir" }
$crt | Copy-Item -Destination (Join-Path $InstallDir 'bin')

# License dialog requires RTF format
$license = (Get-Content (Join-Path $SourceDir 'COPYING_LIB.txt'), (Join-Path $SourceDir 'COPYING.txt')) |
    ForEach-Object { ($_ -replace '([\\{}])', '\$1') + '\par' }
$licenseRtf = Join-Path $work 'license.rtf'
$rtf = '{\rtf1\ansi\deff0{\fonttbl{\f0 Courier New;}}\f0\fs16' + "`r`n" + ($license -join "`r`n") + "`r`n}"
[IO.File]::WriteAllText($licenseRtf, $rtf, [Text.Encoding]::ASCII)

# Collect installed files
$defines = @("-dArch=$Arch", "-dVersion=$version", "-dSourceDir=$SourceDir", "-dLicenseRtf=$licenseRtf")
$sources = @(Join-Path $SourceDir 'win32\freetds.wxs')
foreach ($dir in 'Bin', 'Etc', 'Include', 'Lib') {
    $path = Join-Path $InstallDir $dir.ToLower()
    $wxs = Join-Path $work "$dir.wxs"
    Run (Join-Path $wixBin 'heat.exe') @('dir', $path, '-nologo', '-cg', "${dir}Files", '-dr', "$($dir.ToUpper())DIR",
        '-var', "var.${dir}Dir", '-ag', '-srd', '-sfrag', '-sreg', '-scom', '-o', $wxs)
    $defines += "-d${dir}Dir=$path"
    $sources += $wxs
}

$objDir = (Join-Path $work 'obj') + '\'
Run (Join-Path $wixBin 'candle.exe') (@('-nologo', '-arch', $Arch, '-ext', 'WixUIExtension', '-out', $objDir) + $defines + $sources)
Run (Join-Path $wixBin 'light.exe') @('-nologo', '-ext', 'WixUIExtension', '-cultures:en-us', '-out', $OutFile, (Join-Path $objDir '*.wixobj'))
Write-Host "Created $OutFile version $version"

if (-not $Test) { exit 0 }

# Silent install, check what was installed, silent uninstall
function Msiexec {
    param([string[]]$Arguments)
    $log = Join-Path $work 'msiexec.log'
    $p = Start-Process msiexec.exe -ArgumentList ($Arguments + '/qn', '/l*v', "`"$log`"") -Wait -PassThru
    if ($p.ExitCode -ne 0) {
        Get-Content $log -Tail 60 | Write-Host
        throw "msiexec $Arguments failed with code $($p.ExitCode)"
    }
}

$target = Join-Path $work 'installed'
Msiexec @('/i', "`"$OutFile`"", 'ADDLOCAL=ALL', "INSTALLDIR=`"$target`"")

if ($Arch -in 'x64', 'arm64' -or -not [Environment]::Is64BitOperatingSystem) {
    $key = 'HKLM:\SOFTWARE\ODBC\ODBCINST.INI\FreeTDS'
} else {
    $key = 'HKLM:\SOFTWARE\WOW6432Node\ODBC\ODBCINST.INI\FreeTDS'
}
$driver = (Get-ItemProperty $key).Driver
$expected = Join-Path $target 'bin\tdsodbc.dll'
if ($driver -ne $expected) { throw "ODBC driver registered as '$driver', expected '$expected'" }
if (-not (Test-Path (Join-Path $target 'include\sybdb.h'))) { throw 'Development files not installed' }
if (-not (Test-Path (Join-Path $target 'etc\freetds.conf'))) { throw 'Example configuration not installed' }
Run (Join-Path $target 'bin\tsql.exe') @('-C')

Msiexec @('/x', "`"$OutFile`"")
if (Test-Path (Join-Path $target 'bin\tdsodbc.dll')) { throw 'ODBC driver not removed' }
Write-Host 'Install test passed'
