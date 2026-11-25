$ErrorActionPreference = 'Stop'
$meltdownRoot = Split-Path $PSScriptRoot -Parent
$meltdownVswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$meltdownVs = & $meltdownVswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$meltdownVcvars = Join-Path $meltdownVs 'VC/Auxiliary/Build/vcvars64.bat'
$meltdownEnv = & cmd.exe /d /c "`"$meltdownVcvars`" >nul 2>nul && set"
if ($LASTEXITCODE -ne 0)
{
    throw 'Could not initialize MSVC.'
}
foreach ($meltdownLine in $meltdownEnv)
{
    if ($meltdownLine -match '^([^=]+)=(.*)$')
    {
        [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process')
    }
}
. (Join-Path $meltdownRoot 'cpp/windows-sdk.ps1')
Initialize-ProjectHWindowsSdk
$meltdownTestDir = Join-Path $meltdownRoot 'cpp/.cache/meltdown-tests'
New-Item -ItemType Directory -Force -Path $meltdownTestDir | Out-Null
Push-Location $meltdownTestDir
try
{
    & cl.exe /nologo /std:c++17 /EHsc /W4 /Fe:meltdown_sim_test.exe (Join-Path $PSScriptRoot 'meltdown_sim_test.cpp') (Join-Path $meltdownRoot 'cpp/src/meltdown_sim.cpp') (Join-Path $meltdownRoot 'cpp/src/dungeon_grid.cpp') (Join-Path $meltdownRoot 'cpp/src/meltdown_dungeon.cpp')
    if ($LASTEXITCODE -ne 0)
    {
        exit $LASTEXITCODE
    }
    & .\meltdown_sim_test.exe
    exit $LASTEXITCODE
}
finally
{
    Pop-Location
}
