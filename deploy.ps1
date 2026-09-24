#Requires -Version 5.1
<#
    Install the stereo NGX bridge into a Warhammer III install that already has
    geo-11 (d3d11.dll) and NVIDIA DLSS (nvngx_dlss.dll).

    This repository does NOT redistribute the DLSS mod or geo-11. Point -ModDll
    at your own copy of the Warhammer3DLSS 0.3.1 dinput8.dll; it is installed
    unchanged as wh3dlss393.dll so the bridge can chain it.

    Usage:
      .\deploy.ps1 -GamePath "C:\Program Files (x86)\Steam\steamapps\common\Total War WARHAMMER III" `
                   -ModDll   "C:\path\to\Warhammer3DLSS\dinput8.dll"
#>
param(
    [Parameter(Mandatory)][string] $GamePath,
    [Parameter(Mandatory)][string] $ModDll,
    [switch] $Force
)
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot

$gameHash = 'B7315FA718FD84E2E018E2C4DF06600E9DF0076156B474F148D9D558C939AA55'
$geoHash  = '17374B9D279BD1B1AD3B6BAB67A7690B6896160F46AAFEB811921A2D38535D94'
$dlssHash = '3975567B8943C53ACCE397F2B72380092F84F162D00B0D2C7D08A1025C563983'
$modHash  = 'A12CA4AA0F3D510454B395A1EDC612A3CC1CF6C1BF46F01F74B29A0572252CA2'

if (Get-Process -Name Warhammer3 -ErrorAction SilentlyContinue) { throw 'Warhammer3 is running. Close it first.' }
if (-not (Test-Path -LiteralPath $GamePath)) { throw "Game folder not found: $GamePath" }

$proxy = Join-Path $root 'build\dinput8.dll'
if (-not (Test-Path -LiteralPath $proxy)) { throw "Build output not found: $proxy. Run .\build.ps1 first." }

$expected = [ordered]@{
    (Join-Path $GamePath 'Warhammer3.exe') = $gameHash
    (Join-Path $GamePath 'd3d11.dll')      = $geoHash
    (Join-Path $GamePath 'nvngx_dlss.dll') = $dlssHash
    $ModDll                                = $modHash
}
foreach ($item in $expected.GetEnumerator()) {
    if (-not (Test-Path -LiteralPath $item.Key)) { throw "Missing required file: $($item.Key)" }
    $actual = (Get-FileHash -LiteralPath $item.Key -Algorithm SHA256).Hash
    if ($actual -ne $item.Value) {
        throw "Hash mismatch for $($item.Key)`n  expected $($item.Value)`n  actual   $actual"
    }
}

foreach ($name in 'dinput8.dll', 'wh3dlss393.dll', 'wh3_dlss.ini') {
    if ((Test-Path -LiteralPath (Join-Path $GamePath $name)) -and -not $Force) {
        throw "$name already exists in the game folder. Use -Force to overwrite."
    }
}

Copy-Item -LiteralPath $ModDll -Destination (Join-Path $GamePath 'wh3dlss393.dll') -Force
Copy-Item -LiteralPath $proxy -Destination (Join-Path $GamePath 'dinput8.dll') -Force
Copy-Item -LiteralPath (Join-Path $root 'wh3_dlss.ini') -Destination (Join-Path $GamePath 'wh3_dlss.ini') -Force

if ((Get-FileHash -LiteralPath (Join-Path $GamePath 'dinput8.dll') -Algorithm SHA256).Hash -ne
    (Get-FileHash -LiteralPath $proxy -Algorithm SHA256).Hash) {
    throw 'Deployment verification failed.'
}
Write-Host 'Installed stereo NGX bridge.'
