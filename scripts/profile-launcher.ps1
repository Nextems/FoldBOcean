param([Parameter(Mandatory)][ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Stage,[int]$Trials=3,[int]$Seconds=6,[switch]$Resume)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
$exe=Join-Path $projectRoot 'release\FoldBOcean.exe'
$configFile=Join-Path $env:APPDATA 'com.spiderweb.settings\config.json'
$output=Join-Path $projectRoot ('release\performance\'+$Stage)
if(-not $Resume -and (Test-Path -LiteralPath (Join-Path $output 'config.original.json'))){throw 'This series already exists. Choose a new Stage or use Resume.'}
New-Item -ItemType Directory -Force -Path $output | Out-Null
$original=[IO.File]::ReadAllBytes($(if($Resume){Join-Path $output 'config.original.json'}else{$configFile}))
[IO.File]::WriteAllBytes((Join-Path $output 'config.original.json'),$original)
Add-Type -Path (Join-Path $PSScriptRoot 'perf-counters.cs')
[void][LauncherProbe]::SetProcessDpiAwarenessContext([IntPtr](-4))
$oldCursor=New-Object LauncherProbe+Point
[void][LauncherProbe]::GetCursorPos([ref]$oldCursor)
$rows=[Collections.Generic.List[object]]::new()
if($Resume){foreach($row in (Get-Content -Raw -LiteralPath (Join-Path $output 'raw.json')|ConvertFrom-Json)){$rows.Add($row)}}
$system=Get-CimInstance Win32_ComputerSystem
$logical=[Environment]::ProcessorCount
$counter=$null
$launcher=$null
function Send-Panel([int]$Message,[int]$Key=0){$result=[IntPtr]::Zero;if([LauncherProbe]::SendMessageTimeout($script:window,[uint32]$Message,[IntPtr]$Key,[IntPtr]::Zero,2,10000,[ref]$result) -eq [IntPtr]::Zero){throw 'Panel command timed out'}}
function Stop-Panel {
    $existing=[LauncherProbe]::FindWindow('SpiderWeb.Native.Launcher','FoldBOcean')
    if($existing -ne [IntPtr]::Zero){[void][LauncherProbe]::PostMessage($existing,32776,[IntPtr]::Zero,[IntPtr]::Zero)}
    if($script:launcher){if(-not $script:launcher.WaitForExit(15000)){$script:launcher.Kill();$script:launcher.WaitForExit();Write-Warning 'Benchmark-owned launcher required forced shutdown.'};$script:launcher.Dispose();$script:launcher=$null}
    else {Start-Sleep -Milliseconds 500}
}
function Put-Cursor {
    $rect=New-Object LauncherProbe+Rect
    [void][LauncherProbe]::GetWindowRect($script:window,[ref]$rect)
    if($script:mode -eq 'radial'){$x=($rect.Left+$rect.Right)/2;$y=($rect.Top+$rect.Bottom)/2}
    else {$scale=[Math]::Min(($rect.Right-$rect.Left)/$script:geometry.width,($rect.Bottom-$rect.Top)/$script:geometry.height);$x=$rect.Left+$script:geometry.x*$scale;$y=$rect.Top+$script:geometry.y*$scale+25*$scale}
    [void][LauncherProbe]::SetCursorPos([int]$x,[int]$y)
}
function Sample-Phase([string]$Name,[int]$Duration=$Seconds,[switch]$Cycle) {
    $script:launcher.Refresh();$cpu=$script:launcher.TotalProcessorTime.TotalMilliseconds
    $clock=[Diagnostics.Stopwatch]::StartNew();$counter.Collect();$samples=[Collections.Generic.List[object]]::new();$lastCycle=-1
    while($clock.Elapsed.TotalSeconds -lt $Duration){
        if($Cycle){$cycleIndex=[int][Math]::Floor($clock.Elapsed.TotalSeconds/1.5);if($cycleIndex -ne $lastCycle){if($cycleIndex%2 -eq 0){Send-Panel 32775;Start-Sleep -Milliseconds 100;Put-Cursor;Send-Panel 32773 49}else{Send-Panel 32773 27};$lastCycle=$cycleIndex}}
        Start-Sleep -Milliseconds 500
        $counter.Collect();$script:launcher.Refresh();$sample=[ordered]@{t=$clock.Elapsed.TotalSeconds;visible=[LauncherProbe]::IsWindowVisible($script:window);elements=[LauncherProbe]::ToolCount($script:launcher.Id);workingSetMiB=$script:launcher.WorkingSet64/1MB;privateMiB=$script:launcher.PrivateMemorySize64/1MB;gdi=[LauncherProbe]::GetGuiResources($script:launcher.Handle,0);user=[LauncherProbe]::GetGuiResources($script:launcher.Handle,1);handles=$script:launcher.HandleCount;threads=$script:launcher.Threads.Count}
        foreach($pair in $counter.Read().GetEnumerator()){$sample[$pair.Key]=$pair.Value};$samples.Add([pscustomobject]$sample)
    }
    $script:launcher.Refresh();$cpuDelta=$script:launcher.TotalProcessorTime.TotalMilliseconds-$cpu
    $row=[ordered]@{stage=$Stage;trial=$script:trial;mode=$script:mode;phase=$Name;durationSeconds=$clock.Elapsed.TotalSeconds;cpuMs=$cpuDelta;cpuOneCorePercent=$cpuDelta/($clock.Elapsed.TotalSeconds*10);cpuMachinePercent=$cpuDelta/($clock.Elapsed.TotalSeconds*10*$logical);processPeakWorkingSetMiB=$script:launcher.PeakWorkingSet64/1MB;samples=$samples}
    $rows.Add([pscustomobject]$row)
    $rows | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath (Join-Path $output 'raw.json') -Encoding utf8
    Write-Output ("{0} #{1} {2}/{3}: private={4:N2} MiB, WS={5:N2} MiB, CPU={6:N2} ms" -f $Stage,$script:trial,$script:mode,$Name,($samples.privateMiB|Measure-Object -Average).Average,($samples.workingSetMiB|Measure-Object -Average).Average,$cpuDelta)
}
try {
    Stop-Panel
    for($trial=1;$trial -le $Trials;$trial++){
        foreach($mode in @('radial','web')){
            if(@($rows|Where-Object {$_.trial -eq $trial -and $_.mode -eq $mode -and $_.phase -eq 'expanded-rgb'}).Count){continue}
            $cfg=[Text.Encoding]::UTF8.GetString($original).TrimStart([char]0xfeff)|ConvertFrom-Json
            $cfg.launcher.view=$mode;$cfg.rgb.radial=$false;$cfg.rgb.blocks=$false;$cfg.rgb.branches=$false
            $cfg|ConvertTo-Json -Depth 40|Set-Content -LiteralPath $configFile -Encoding utf8NoBOM
            $geometry=(& node (Join-Path $PSScriptRoot 'perf-geometry.mjs') $configFile)|ConvertFrom-Json
            if($LASTEXITCODE -ne 0){throw 'Geometry failed'}
            $launcher=Start-Process -FilePath $exe -ArgumentList '--background' -WindowStyle Hidden -PassThru
            Start-Sleep -Seconds 1
            $window=[LauncherProbe]::FindWindow('SpiderWeb.Native.Launcher','FoldBOcean')
            if($window -eq [IntPtr]::Zero){throw 'No launcher window'}
            $counter=[LauncherCounters]::new()
            Sample-Phase 'tray-cold'
            try {[LauncherProbe]::BeginSweep($false);Sample-Phase 'tray-mouse'}finally{[LauncherProbe]::EndSweep()}
            Send-Panel 32775;Start-Sleep -Milliseconds 150;Put-Cursor;Start-Sleep -Seconds 1
            Sample-Phase 'root'
            Send-Panel 32773 49;Start-Sleep -Seconds 1
            Sample-Phase 'expanded'
            Sample-Phase 'open-close' 12 -Cycle
            Send-Panel 32773 27;Start-Sleep -Seconds 1
            Sample-Phase 'tray-warm'
            $cfg.rgb.radial=$true;$cfg.rgb.blocks=$true;$cfg.rgb.branches=$true
            $cfg|ConvertTo-Json -Depth 40|Set-Content -LiteralPath $configFile -Encoding utf8NoBOM
            Start-Sleep -Seconds 2
            Send-Panel 32775;Start-Sleep -Milliseconds 150;Put-Cursor;Send-Panel 32773 49;Start-Sleep -Seconds 1
            Sample-Phase 'expanded-rgb'
            $counter.Dispose();$counter=$null;Stop-Panel
        }
    }
}finally{
    try {if($counter){$counter.Dispose()};Stop-Panel}finally{
        [IO.File]::WriteAllBytes($configFile,$original)
        [void][LauncherProbe]::SetCursorPos($oldCursor.X,$oldCursor.Y)
        Start-Process -FilePath $exe -ArgumentList '--background' -WindowStyle Hidden
    }
}
[ordered]@{stage=$Stage;date=(Get-Date -Format o);logicalProcessors=$logical;physicalMemoryGiB=$system.TotalPhysicalMemory/1GB;seconds=$Seconds;trials=$Trials;exeHash=(Get-FileHash -LiteralPath $exe).Hash;configHash=(Get-FileHash -LiteralPath (Join-Path $output 'config.original.json')).Hash}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $output 'metadata.json') -Encoding utf8
