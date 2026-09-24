#Requires -Version 5.1
# Build and run the bridge core unit tests under AddressSanitizer.
#
# Usage:  .\run_asan_tests.ps1
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'find-vcvars.ps1')

$vcvars = Find-Vcvars
$root = $PSScriptRoot
$out = Join-Path $root 'build'
New-Item -ItemType Directory -Path $out -Force | Out-Null

# cl and the ASan runtime live in the same bin directory, so running the test in
# the same command shell after vcvars resolves the runtime automatically.
$command = 'call "' + $vcvars + '" >nul && cd /d "' + $out + '" && ' +
    'cl /nologo /std:c++17 /EHsc /W4 /WX /fsanitize=address /Zi "' + (Join-Path $root 'bridge_core.cpp') + '" "' +
    (Join-Path $root 'bridge_core_tests.cpp') + '" /Fe:bridge_core_tests_asan.exe && bridge_core_tests_asan.exe'
& cmd.exe /c $command
if ($LASTEXITCODE -ne 0) { throw "ASan tests exited with $LASTEXITCODE" }
