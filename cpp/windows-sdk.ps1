function Initialize-ProjectHWindowsSdk {
    $taskCache = Join-Path $PSScriptRoot '.cache'
    $taskSdkPackageVersion = '10.0.26100.9169'
    $taskSdkVersion = '10.0.26100.0'
    $taskIncludeRoot = Join-Path $taskCache "sdk-headers/c/Include/$taskSdkVersion"
    $taskLibRoot = Join-Path $taskCache 'sdk-x64/c'
    foreach ($taskPackage in @(
        @{ Name = 'microsoft.windows.sdk.cpp'; Folder = 'sdk-headers' },
        @{ Name = 'microsoft.windows.sdk.cpp.x64'; Folder = 'sdk-x64' }
    )) {
        $taskFolder = Join-Path $taskCache $taskPackage.Folder
        if (!(Test-Path -LiteralPath $taskFolder)) {
                New-Item -ItemType Directory -Path $taskCache -Force | Out-Null
                $taskZip = Join-Path $taskCache ($taskPackage.Folder + '.zip')
                $taskPackageName = $taskPackage.Name
                Write-Host "Downloading Microsoft Windows SDK: $taskPackageName"
                Invoke-WebRequest -UseBasicParsing -Uri "https://api.nuget.org/v3-flatcontainer/$taskPackageName/$taskSdkPackageVersion/$taskPackageName.$taskSdkPackageVersion.nupkg" -OutFile $taskZip
                Expand-Archive -LiteralPath $taskZip -DestinationPath $taskFolder -Force
        }
    }
    if (!(Test-Path -LiteralPath "$taskIncludeRoot/ucrt/stdio.h") -or !(Test-Path -LiteralPath "$taskLibRoot/um/x64/kernel32.lib")) {
        throw 'The local Windows SDK is incomplete. Restore the SDK packages in cpp/.cache.'
    }
    $env:INCLUDE = "$taskIncludeRoot/ucrt;$taskIncludeRoot/shared;$taskIncludeRoot/um;$taskIncludeRoot/winrt;" + $env:INCLUDE
    $env:LIB = "$taskLibRoot/ucrt/x64;$taskLibRoot/um/x64;" + $env:LIB
    $env:PATH = (Join-Path $taskCache "sdk-headers/c/bin/$taskSdkVersion/x64") + ';' + $env:PATH
}
