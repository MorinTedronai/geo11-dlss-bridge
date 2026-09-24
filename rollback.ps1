#Requires -Version 5.1
<#
    Remove the stereo NGX bridge from a Warhammer III install.

    Usage:
      .\rollback.ps1 -GamePath "C:\Program Files (x86)\Steam\steamapps\common\Total War WARHAMMER III"
#>
param(
    [Parameter(Mandatory)][string] $GamePath
)
$ErrorActionPreference = 'Stop'

if (Get-Process -Name Warhammer3 -ErrorAction SilentlyContinue) { throw 'Warhammer3 is running. Close it first.' }

foreach ($name in 'dinput8.dll', 'wh3dlss393.dll', 'wh3_dlss.ini', 'geo11-dlss-bridge.log') {
    Remove-Item -LiteralPath (Join-Path $GamePath $name) -ErrorAction SilentlyContinue
}

foreach ($name in 'dinput8.dll', 'wh3dlss393.dll', 'wh3_dlss.ini') {
    if (Test-Path -LiteralPath (Join-Path $GamePath $name)) { throw "Rollback left $name behind." }
}
Write-Host 'Removed stereo NGX bridge.'
