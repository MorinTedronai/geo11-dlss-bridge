#Requires -Version 5.1
# Build the stereo NGX bridge (build/dinput8.dll).
#
# Requirements:
#   * Visual Studio 2022 (x64 C++ toolset); located automatically via vswhere.
#   * MinHook checked out into third_party/minhook, or set $env:MINHOOK_DIR.
#
# Usage:  .\build.ps1
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'find-vcvars.ps1')

$vcvars = Find-Vcvars
$root = $PSScriptRoot
$minhook = if ($env:MINHOOK_DIR) { $env:MINHOOK_DIR } else { Join-Path $root 'third_party\minhook' }
if (-not (Test-Path -LiteralPath (Join-Path $minhook 'include\MinHook.h'))) {
    throw "MinHook not found at '$minhook'. Clone it into third_party\minhook or set MINHOOK_DIR."
}

$out = Join-Path $root 'build'
New-Item -ItemType Directory -Path $out -Force | Out-Null

$assemble = 'ml64 /nologo /c /Fo "' + (Join-Path $out 'forwarders.obj') + '" "' + (Join-Path $root 'forwarders.asm') + '"'
$compile = @(
    'cl /nologo /std:c++17 /O2 /EHsc /W4 /LD',
    ('/I"' + (Join-Path $root 'abi') + '"'),
    ('/I"' + $root + '"'),
    ('/I"' + (Join-Path $minhook 'include') + '"'),
    ('"' + (Join-Path $root 'proxy.cpp') + '"'),
    ('"' + (Join-Path $root 'bridge_core.cpp') + '"'),
    ('"' + (Join-Path $root 'geo11_adapter.cpp') + '"'),
    ('"' + (Join-Path $minhook 'src\buffer.c') + '"'),
    ('"' + (Join-Path $minhook 'src\hook.c') + '"'),
    ('"' + (Join-Path $minhook 'src\trampoline.c') + '"'),
    ('"' + (Join-Path $minhook 'src\hde\hde64.c') + '"'),
    ('"' + (Join-Path $out 'forwarders.obj') + '"'),
    ('/link /DEF:"' + (Join-Path $root 'dinput8_proxy.def') + '" /OUT:"' + (Join-Path $out 'dinput8.dll') + '" d3d11.lib bcrypt.lib')
) -join ' '

$command = 'call "' + $vcvars + '" >nul && cd /d "' + $out + '" && ' + $assemble + ' && ' + $compile
& cmd.exe /c $command
if ($LASTEXITCODE -ne 0) { throw "Compiler exited with $LASTEXITCODE" }

Write-Host "Built $(Join-Path $out 'dinput8.dll')"
