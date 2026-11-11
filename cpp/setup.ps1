param([string]$PythonCommand = 'python')
$ErrorActionPreference = 'Stop'
$taskSha = 'cedbd9e71b24eb2898b28867c5faf6114da9e5e2'
$taskVendor = Join-Path $PSScriptRoot 'godot-cpp'
$taskCache = Join-Path $PSScriptRoot '.cache'
New-Item -ItemType Directory -Path "$taskCache/temp" -Force | Out-Null
$env:TEMP = "$taskCache/temp"
$env:TMP = $env:TEMP
if (!(Test-Path -LiteralPath $taskVendor)) {
    $taskZip = Join-Path $taskCache "godot-cpp-$taskSha.zip"
    Invoke-WebRequest -Uri "https://codeload.github.com/godotengine/godot-cpp/zip/$taskSha" -OutFile $taskZip
    Expand-Archive -LiteralPath $taskZip -DestinationPath $taskCache -Force
    $taskSource = Join-Path $taskCache "godot-cpp-$taskSha"
    # Copy the pinned source snapshot without adding an embedded Git repository.
    Copy-Item -LiteralPath $taskSource -Destination $taskVendor -Recurse
}
$taskPython = Join-Path $PSScriptRoot '.venv/Scripts/python.exe'
if (!(Test-Path -LiteralPath $taskPython)) {
    & $PythonCommand -m venv (Join-Path $PSScriptRoot '.venv')
    if ($LASTEXITCODE -ne 0) { throw 'Python 3 is required. Pass its path with -PythonCommand.' }
}
& $taskPython -m ensurepip --upgrade
if ($LASTEXITCODE -ne 0) { throw 'Unable to install pip in the local Python environment.' }
& $taskPython -m pip install --disable-pip-version-check --no-cache-dir -r (Join-Path $PSScriptRoot 'requirements.txt')
if ($LASTEXITCODE -ne 0) { throw 'Unable to install SCons.' }
Write-Host 'Ready. Open ProjectH.sln in Visual Studio and build Debug | x64.'
