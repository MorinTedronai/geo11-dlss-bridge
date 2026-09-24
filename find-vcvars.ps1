#Requires -Version 5.1
# Locate the Visual Studio 2022 x64 build environment (vcvars64.bat).
function Find-Vcvars {
    $candidates = @()

    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere) {
        & $vswhere -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath |
            ForEach-Object { if ($_) { $candidates += (Join-Path $_ 'VC\Auxiliary\Build\vcvars64.bat') } }
    }

    $candidates += @(
        'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat',
        'C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat',
        'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat'
    )

    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate) { return $candidate }
    }
    throw 'vcvars64.bat not found. Install Visual Studio 2022 with the Desktop C++ workload.'
}
