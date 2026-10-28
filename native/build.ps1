param([switch]$NativeOnly, [switch]$Test)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vsInstall = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsInstall) { throw 'Install Visual Studio C++ Desktop Build Tools.' }
$devcmd = Join-Path $vsInstall 'Common7\Tools\VsDevCmd.bat'
$buildDir = Join-Path $PSScriptRoot 'build'
$outputDir = Join-Path $projectRoot 'release'
New-Item -ItemType Directory -Force $buildDir,$outputDir | Out-Null
Push-Location $PSScriptRoot
try {
    $buildCommand = 'call "' + $devcmd + '" -arch=x64 -host_arch=x64 >nul && rc /nologo /fo build\app.res app.rc && cl /nologo /std:c++17 /utf-8 /O2 /MT /EHsc /DUNICODE /D_UNICODE /DNOMINMAX /W4 /Fo:build\ /Fe:..\release\FoldBOcean.exe main.cpp build\app.res /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib gdiplus.lib shell32.lib ole32.lib advapi32.lib comctl32.lib version.lib dwmapi.lib'
    & cmd.exe /d /c $buildCommand
    if ($LASTEXITCODE -ne 0) { throw 'Native launcher build failed.' }
    if ($Test) {
        $testCommand = 'call "' + $devcmd + '" -arch=x64 -host_arch=x64 >nul && cl /nologo /std:c++17 /utf-8 /O2 /MT /EHsc /DUNICODE /D_UNICODE /Fo:build\ /Fe:..\release\SpiderWeb.TestFixture.exe test-fixture.cpp /link /SUBSYSTEM:WINDOWS user32.lib'
        & cmd.exe /d /c $testCommand
        if ($LASTEXITCODE -ne 0) { throw 'Test fixture build failed.' }
        $testProcess = Start-Process -FilePath (Join-Path $outputDir 'FoldBOcean.exe') -ArgumentList '--self-test' -WindowStyle Hidden -Wait -PassThru
        if ($testProcess.ExitCode -ne 0) { throw 'Native self-tests failed; see release/native-tests.json.' }
        Get-Content -LiteralPath (Join-Path $outputDir 'native-tests.json')
        Remove-Item -LiteralPath (Join-Path $outputDir 'SpiderWeb.TestFixture.exe') -Force
    }
    $settingsExe = Join-Path $projectRoot 'src-tauri\target\release\spiderweb-settings.exe'
    if (-not $NativeOnly -and (Test-Path -LiteralPath $settingsExe)) { Copy-Item -LiteralPath $settingsExe -Destination (Join-Path $outputDir 'FoldBOcean Settings.exe') -Force }
} finally { Pop-Location }
