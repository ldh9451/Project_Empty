param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug',
    [switch]$Clean,
    [switch]$Rebuild
)
$ErrorActionPreference = 'Stop'
$taskPython = Join-Path $PSScriptRoot '.venv/Scripts/python.exe'
if (!(Test-Path -LiteralPath $taskPython) -or !(Test-Path -LiteralPath (Join-Path $PSScriptRoot 'godot-cpp/SConstruct'))) {
    throw 'Run cpp/setup.ps1 first to restore Python and godot-cpp dependencies.'
}
$taskVswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$taskVs = & $taskVswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$taskVs) { throw 'Install Visual Studio Desktop development with C++ and Windows SDK.' }
$taskVcvars = Join-Path $taskVs 'VC/Auxiliary/Build/vcvars64.bat'
$taskEnv = & cmd.exe /d /c "`"$taskVcvars`" >nul 2>nul && set"
if ($LASTEXITCODE -ne 0) { throw 'Unable to initialize the Visual Studio compiler.' }
foreach ($taskLine in $taskEnv) {
    if ($taskLine -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process') }
}
$taskTemp = Join-Path $PSScriptRoot '.cache/temp'
New-Item -ItemType Directory -Path $taskTemp -Force | Out-Null
$env:TEMP = $taskTemp
$env:TMP = $taskTemp
$env:PYTHONUTF8 = '1'
$env:PYTHONIOENCODING = 'utf-8'
. (Join-Path $PSScriptRoot 'windows-sdk.ps1')
Initialize-ProjectHWindowsSdk
$taskTarget = if ($Configuration -eq 'Release') { 'template_release' } else { 'template_debug' }
$taskJobs = [Math]::Max(1, [Math]::Min(8, [Environment]::ProcessorCount))
$taskArgs = @('-m', 'SCons', 'platform=windows', "target=$taskTarget", 'arch=x86_64', 'build_profile=build_profile.json', 'debug_symbols=no', "-j$taskJobs")
if ($Configuration -eq 'Debug') { $taskArgs += 'optimize=none' }
Push-Location $PSScriptRoot
try {
    if ($Clean -or $Rebuild) {
        & $taskPython @taskArgs -c
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        if ($Clean) { exit 0 }
    }
    & $taskPython @taskArgs
    exit $LASTEXITCODE
} finally { Pop-Location }
