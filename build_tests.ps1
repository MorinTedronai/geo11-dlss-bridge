#Requires -Version 5.1
# Build and run the CPU-only bridge core unit tests.
#
# Usage:  .\build_tests.ps1
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'find-vcvars.ps1')

$vcvars = Find-Vcvars
$root = $PSScriptRoot
$out = Join-Path $root 'build'
New-Item -ItemType Directory -Path $out -Force | Out-Null

$command = 'call "' + $vcvars + '" >nul && cd /d "' + $out + '" && ' +
    'cl /nologo /std:c++17 /EHsc /W4 /WX "' + (Join-Path $root 'bridge_core.cpp') + '" "' +
    (Join-Path $root 'bridge_core_tests.cpp') + '" /Fe:bridge_core_tests.exe && bridge_core_tests.exe'
& cmd.exe /c $command
if ($LASTEXITCODE -ne 0) { throw "Tests exited with $LASTEXITCODE" }
