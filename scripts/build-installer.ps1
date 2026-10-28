$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
Push-Location $projectRoot
$nativeExe=Join-Path $projectRoot 'release\FoldBOcean.exe'
$wasRunning=@(Get-Process FoldBOcean -ErrorAction SilentlyContinue|Where-Object {$_.Path -eq $nativeExe}).Count -gt 0
try {
    if(Test-Path -LiteralPath $nativeExe){
        Start-Process -FilePath $nativeExe -ArgumentList '--quit-all' -WindowStyle Hidden -Wait
        foreach($process in @(Get-Process FoldBOcean -ErrorAction SilentlyContinue|Where-Object {$_.Path -eq $nativeExe})){
            if(-not $process.WaitForExit(5000)){throw 'Завершите работающую панель перед сборкой.'}
        }
    }
    & powershell -NoProfile -ExecutionPolicy Bypass -File native/build.ps1 -NativeOnly
    if($LASTEXITCODE -ne 0){throw 'Не удалось собрать панель'}
    & npm.cmd run tauri build -- --config src-tauri/tauri.installer.conf.json --bundles nsis
    if($LASTEXITCODE -ne 0){throw 'Не удалось собрать установщик'}
    $version=(Get-Content -Raw src-tauri/tauri.conf.json|ConvertFrom-Json).version
    $bundle=Join-Path $projectRoot "src-tauri\target\release\bundle\nsis\FoldBOcean_${version}_x64-setup.exe"
    if(-not(Test-Path -LiteralPath $bundle)){throw "Установщик не найден: $bundle"}
    $output=Join-Path $projectRoot "release\FoldBOcean-${version}-Setup-x64.exe"
    Copy-Item -LiteralPath $bundle -Destination $output -Force
    Copy-Item -LiteralPath 'src-tauri\target\release\spiderweb-settings.exe' -Destination 'release\FoldBOcean Settings.exe' -Force
    Copy-Item -LiteralPath $nativeExe -Destination 'release\SpiderWeb.exe' -Force
    Copy-Item -LiteralPath 'release\FoldBOcean Settings.exe' -Destination 'release\SpiderWeb Settings.exe' -Force
    $hash=(Get-FileHash -LiteralPath $output -Algorithm SHA256).Hash.ToLowerInvariant()
    "$hash  $([IO.Path]::GetFileName($output))"|Set-Content -LiteralPath "$output.sha256" -Encoding ascii
    Write-Output "Готово: $output"
} finally {
    if($wasRunning -and (Test-Path -LiteralPath $nativeExe)){Start-Process -FilePath $nativeExe -ArgumentList '--background' -WindowStyle Hidden}
    Pop-Location
}
