$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
$exe=Join-Path $projectRoot 'release\FoldBOcean.exe'
Add-Type -Path (Join-Path $PSScriptRoot 'perf-counters.cs')
$config=Get-Content -Raw -LiteralPath (Join-Path $env:APPDATA 'com.spiderweb.settings\config.json')|ConvertFrom-Json
if($config.launcher.trigger.type -ne 'keyboard' -or $config.launcher.mode -ne 'hold'){throw 'This check requires a keyboard hold binding.'}
$key=[int]$config.launcher.trigger.key
$modifiers=@();if($config.launcher.trigger.modifiers -band 1){$modifiers+=17};if($config.launcher.trigger.modifiers -band 2){$modifiers+=18};if($config.launcher.trigger.modifiers -band 4){$modifiers+=16}
$cursor=New-Object LauncherProbe+Point;[void][LauncherProbe]::GetCursorPos([ref]$cursor)
function Stop-Panel {
    Start-Process -FilePath $exe -ArgumentList '--quit' -WindowStyle Hidden -Wait
    foreach($process in @(Get-Process FoldBOcean -ErrorAction SilentlyContinue|Where-Object {$_.Path -eq $exe})){if(-not $process.WaitForExit(5000)){throw 'Launcher did not shut down'}}
}
try {
    for($iteration=1;$iteration -le 3;$iteration++){
        Stop-Panel
        $process=Start-Process -FilePath $exe -ArgumentList '--background' -WindowStyle Hidden -PassThru
        $window=[IntPtr]::Zero
        for($attempt=0;$attempt -lt 100 -and $window -eq [IntPtr]::Zero;$attempt++){Start-Sleep -Milliseconds 30;$window=[LauncherProbe]::FindWindow('SpiderWeb.Native.Launcher','FoldBOcean')}
        if($window -eq [IntPtr]::Zero){throw 'No launcher window after restart'}
        Start-Sleep -Milliseconds 400
        foreach($modifier in $modifiers){[LauncherProbe]::Key($modifier,$false)}
        Start-Sleep -Milliseconds 100;[LauncherProbe]::Key($key,$false)
        for($attempt=0;$attempt -lt 30 -and -not [LauncherProbe]::IsWindowVisible($window);$attempt++){Start-Sleep -Milliseconds 30}
        if(-not [LauncherProbe]::IsWindowVisible($window)){throw "Bind did not open the panel after restart $iteration"}
        $rect=New-Object LauncherProbe+Rect;[void][LauncherProbe]::GetWindowRect($window,[ref]$rect);[void][LauncherProbe]::SetCursorPos(($rect.Left+$rect.Right)/2,($rect.Top+$rect.Bottom)/2)
        [LauncherProbe]::Key($key,$true);foreach($modifier in $modifiers){[LauncherProbe]::Key($modifier,$true)}
        for($attempt=0;$attempt -lt 30 -and [LauncherProbe]::IsWindowVisible($window);$attempt++){Start-Sleep -Milliseconds 30}
        if([LauncherProbe]::IsWindowVisible($window)){throw "Hold release did not close the panel after restart $iteration"}
    }
    Write-Output 'PASS: three process restarts; configured keyboard binding opens and releases the panel each time.'
} finally {
    [LauncherProbe]::Key($key,$true);foreach($modifier in $modifiers){[LauncherProbe]::Key($modifier,$true)}
    if($window -ne [IntPtr]::Zero){[void][LauncherProbe]::PostMessage($window,32773,[IntPtr]27,[IntPtr]::Zero)}
    [void][LauncherProbe]::SetCursorPos($cursor.X,$cursor.Y)
}
