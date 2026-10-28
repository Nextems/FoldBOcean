param([Parameter(Mandatory)][string]$Stage)
$ErrorActionPreference='Stop'
Add-Type -Path (Join-Path $PSScriptRoot 'perf-counters.cs')
$launcher=Get-Process -Name FoldBOcean
if(@($launcher).Count -ne 1){throw 'Expected one hidden launcher'}
$window=[LauncherProbe]::FindWindow('SpiderWeb.Native.Launcher','FoldBOcean')
if([LauncherProbe]::IsWindowVisible($window)){throw 'Hide the panel before measuring idle input'}
$origin=New-Object LauncherProbe+Point;[void][LauncherProbe]::GetCursorPos([ref]$origin)
$rows=@();$counter=[LauncherCounters]::new()
try{
    for($trial=1;$trial -le 3;$trial++){
        $launcher.Refresh();$cpu=$launcher.TotalProcessorTime.TotalMilliseconds;$counter.Collect();$watch=[Diagnostics.Stopwatch]::StartNew();$samples=@()
        try{
            [LauncherProbe]::BeginSweep($true)
            for($i=0;$i -lt 6;$i++){Start-Sleep -Milliseconds 700;$counter.Collect();$samples+=,$counter.Read()}
        }finally{[LauncherProbe]::EndSweep()}
        $launcher.Refresh();$rows+=,[ordered]@{trial=$trial;seconds=$watch.Elapsed.TotalSeconds;cpuMs=$launcher.TotalProcessorTime.TotalMilliseconds-$cpu;samples=$samples}
    }
}finally{$counter.Dispose();[LauncherProbe]::EndSweep();[void][LauncherProbe]::SetCursorPos($origin.X,$origin.Y)}
$output=Join-Path (Split-Path -Parent $PSScriptRoot) ('release\performance\'+$Stage+'\mouse-idle.json')
$rows|ConvertTo-Json -Depth 8|Set-Content -LiteralPath $output -Encoding utf8
$rows|ForEach-Object{[pscustomobject]@{trial=$_.trial;cpuMs=$_.cpuMs;contextSwitchesPerSec=($_.samples.launcherContextSwitchesPerSec|Measure-Object -Average).Average}}|Format-Table
