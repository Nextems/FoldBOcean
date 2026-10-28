$ErrorActionPreference='Stop'
Add-Type -Path (Join-Path $PSScriptRoot 'perf-counters.cs')
[void][LauncherProbe]::SetProcessDpiAwarenessContext([IntPtr](-4))
$configFile=Join-Path $env:APPDATA 'com.spiderweb.settings\config.json'
$original=[IO.File]::ReadAllBytes($configFile)
$window=[LauncherProbe]::FindWindow('SpiderWeb.Native.Launcher','FoldBOcean')
if($window -eq [IntPtr]::Zero){throw 'Start the native launcher first'}
$origin=New-Object LauncherProbe+Point;[void][LauncherProbe]::GetCursorPos([ref]$origin)
function Wait-Visible([bool]$Expected){for($i=0;$i -lt 60;$i++){if([LauncherProbe]::IsWindowVisible($window) -eq $Expected){return};Start-Sleep -Milliseconds 50};throw "Visibility did not become $Expected"}
function Center-Pointer {$rect=New-Object LauncherProbe+Rect;[void][LauncherProbe]::GetWindowRect($window,[ref]$rect);[void][LauncherProbe]::SetCursorPos(($rect.Left+$rect.Right)/2,($rect.Top+$rect.Bottom)/2)}
function Configure([string]$Type,[string]$Mode){
    $config=[Text.Encoding]::UTF8.GetString($original).TrimStart([char]0xfeff)|ConvertFrom-Json
    $config.launcher.view='radial';$config.launcher.mode=$Mode
    $config.launcher.trigger=if($Type -eq 'keyboard'){@{type='keyboard';key=135;modifiers=0;label='F24'}}else{@{type='mouse';button='middle'}}
    $config|ConvertTo-Json -Depth 40|Set-Content -LiteralPath $configFile -Encoding utf8NoBOM
    [void][LauncherProbe]::PostMessage($window,32774,[IntPtr]::Zero,[IntPtr]::Zero)
    Start-Sleep -Milliseconds 700
}
try{
    [void][LauncherProbe]::PostMessage($window,32773,[IntPtr]27,[IntPtr]::Zero);Wait-Visible $false
    Configure 'keyboard' 'hold'
    [LauncherProbe]::Key(135,$false);Wait-Visible $true;Center-Pointer;[LauncherProbe]::Key(135,$true);Wait-Visible $false
    Configure 'keyboard' 'toggle'
    [LauncherProbe]::Key(135,$false);Wait-Visible $true;Center-Pointer;[LauncherProbe]::Key(135,$true);Start-Sleep -Milliseconds 300;Wait-Visible $true
    [LauncherProbe]::Key(135,$false);Wait-Visible $false;[LauncherProbe]::Key(135,$true)
    Configure 'mouse' 'hold'
    [LauncherProbe]::MiddleButton($false);Wait-Visible $true;Center-Pointer;[LauncherProbe]::MiddleButton($true);Wait-Visible $false
    Configure 'keyboard' 'hold'
    [LauncherProbe]::Key(135,$false);Wait-Visible $true;Center-Pointer;[LauncherProbe]::Key(135,$true);Wait-Visible $false
    Write-Output 'PASS: keyboard hold/toggle, middle-button hold, live trigger changes.'
}finally{
    [LauncherProbe]::Key(135,$true);[LauncherProbe]::MiddleButton($true)
    [void][LauncherProbe]::PostMessage($window,32773,[IntPtr]27,[IntPtr]::Zero)
    [IO.File]::WriteAllBytes($configFile,$original)
    [void][LauncherProbe]::PostMessage($window,32774,[IntPtr]::Zero,[IntPtr]::Zero)
    [void][LauncherProbe]::SetCursorPos($origin.X,$origin.Y)
}
