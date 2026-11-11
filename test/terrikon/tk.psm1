# Terrikon test helpers: UI Automation (find and press controls without the
# mouse), rc commands, process health. Import-Module E:\VLC-fork\scripts\tk.psm1
Add-Type -AssemblyName UIAutomationClient, UIAutomationTypes
Add-Type @'
using System; using System.Runtime.InteropServices;
public static class TkUia {
    [DllImport("user32")] public static extern bool IsHungAppWindow(IntPtr h);
    [DllImport("user32")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, IntPtr e);
    public delegate bool P(IntPtr h, IntPtr l);
    [DllImport("user32")] public static extern bool EnumWindows(P p, IntPtr l);
    [DllImport("user32")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
    [DllImport("user32")] public static extern bool IsWindowVisible(IntPtr h);
}
'@ -ErrorAction SilentlyContinue

$script:Exe = 'E:\VLC-fork\terrikon\terrikon.exe'
$script:Log = 'E:\VLC-fork\pipeline-test\ui-log.txt'

function Get-TkProcess { Get-Process terrikon -ErrorAction SilentlyContinue | Where-Object { -not $_.HasExited } | Sort-Object StartTime | Select-Object -Last 1 }

function Start-Tk([string]$File, [switch]$RealProfile, [string[]]$ExtraArgs = @()) {
    Remove-Item $script:Log -ErrorAction SilentlyContinue
    # A profile of its own (settings, media library, history), so tests leave
    # the user's alone
    $saved = $env:APPDATA
    if (-not $RealProfile) { New-Item -ItemType Directory -Force 'E:\VLC-fork\test-profile' | Out-Null; $env:APPDATA = 'E:\VLC-fork\test-profile' }
    Start-Process $script:Exe -WorkingDirectory (Split-Path $script:Exe) -ArgumentList (@(
        '--no-qt-video-autoresize', '--extraintf=rc', '--rc-host=127.0.0.1:4212', '--rc-quiet',
        '--file-logging', '--log-verbose=1', "--logfile=$script:Log") + $ExtraArgs) | Out-Null
    $env:APPDATA = $saved
    for ($i = 0; $i -lt 40; $i++) { Start-Sleep -Milliseconds 500; if (Test-TkRc) { break } }
    if ($File) { Send-Tk "add $File" | Out-Null }
}

function Test-TkRc { try { $c = New-Object Net.Sockets.TcpClient; $c.Connect('127.0.0.1', 4212); $c.Close(); $true } catch { $false } }

# Sends rc commands, returns the answer text (empty when rc does not answer)
function Send-Tk([string[]]$Commands, [int]$WaitMs = 300) {
    $out = ''
    try {
        $c = New-Object Net.Sockets.TcpClient
        $c.ReceiveTimeout = 2000
        $c.Connect('127.0.0.1', 4212)
        $s = $c.GetStream(); $w = New-Object IO.StreamWriter($s); $w.AutoFlush = $true
        foreach ($cmd in $Commands) { $w.WriteLine($cmd); Start-Sleep -Milliseconds $WaitMs }
        $buf = New-Object byte[] 65536
        while ($s.DataAvailable) { $n = $s.Read($buf, 0, $buf.Length); $out += [Text.Encoding]::UTF8.GetString($buf, 0, $n) }
        $c.Close()
    } catch { }
    $out
}

function Get-TkTime { $a = Send-Tk 'get_time'; if ($a -match '(-?\d+)') { [int]$Matches[1] } else { $null } }

function Get-TkUiadows([int]$ProcessId) {
    $c = New-Object Windows.Automation.PropertyCondition([Windows.Automation.AutomationElement]::ProcessIdProperty, $ProcessId)
    [Windows.Automation.AutomationElement]::RootElement.FindAll([Windows.Automation.TreeScope]::Children, $c)
}

# Finds a control by its accessible name or automation id (exact first, then
# "contains"), in every window of Terrikon
function Find-Tk([string]$Name, [string]$Id, [string]$Type, [int]$TimeoutMs = 3000) {
    $p = Get-TkProcess; if (-not $p) { return $null }
    $deadline = (Get-Date).AddMilliseconds($TimeoutMs)
    do {
        foreach ($w in Get-TkUiadows $p.Id) {
            $all = $w.FindAll([Windows.Automation.TreeScope]::Descendants, [Windows.Automation.Condition]::TrueCondition)
            $hits = @($all | Where-Object {
                ((-not $Id) -or $_.Current.AutomationId -eq $Id) -and
                ((-not $Name) -or $_.Current.Name -eq $Name) -and
                ((-not $Type) -or $_.Current.ControlType.ProgrammaticName -like "*$Type") })
            if (-not $hits -and $Name) {
                $hits = @($all | Where-Object { $_.Current.Name -like "*$Name*" -and ((-not $Type) -or $_.Current.ControlType.ProgrammaticName -like "*$Type") })
            }
            if (-not $Id) { $hits = @($hits | Where-Object { -not $_.Current.IsOffscreen }) }
            if ($hits) { return $hits[0] }
        }
        Start-Sleep -Milliseconds 200
    } while ((Get-Date) -lt $deadline)
    $null
}

# Presses a control: the Invoke pattern, else a real click on its centre
function Invoke-Tk([string]$Name, [string]$Id, [string]$Type, [switch]$Click, [switch]$Double, [switch]$Right) {
    $e = Find-Tk -Name $Name -Id $Id -Type $Type
    if (-not $e) { Write-Warning "not found: $Name$Id"; return $false }
    $pat = $null
    if (-not $Click -and -not $Double -and -not $Right -and $e.TryGetCurrentPattern([Windows.Automation.InvokePattern]::Pattern, [ref]$pat)) {
        $pat.Invoke(); return $true
    }
    $r = $e.Current.BoundingRectangle
    $p = Get-TkProcess; [TkUia]::SetForegroundWindow($p.MainWindowHandle) | Out-Null
    [TkUia]::SetCursorPos([int]($r.X + $r.Width / 2), [int]($r.Y + $r.Height / 2)) | Out-Null
    Start-Sleep -Milliseconds 80
    $down, $up = if ($Right) { 8, 16 } else { 2, 4 }
    $times = if ($Double) { 2 } else { 1 }
    for ($i = 0; $i -lt $times; $i++) { [TkUia]::mouse_event($down, 0, 0, 0, [IntPtr]::Zero); [TkUia]::mouse_event($up, 0, 0, 0, [IntPtr]::Zero); Start-Sleep -Milliseconds 60 }
    $true
}

# Names of the visible controls (to write a scenario)
function Get-TkControls([string]$Filter = '*') {
    $p = Get-TkProcess
    foreach ($w in Get-TkUiadows $p.Id) {
        $w.FindAll([Windows.Automation.TreeScope]::Descendants, [Windows.Automation.Condition]::TrueCondition) |
            Where-Object { -not $_.Current.IsOffscreen -and ($_.Current.Name -like $Filter -or $_.Current.AutomationId -like $Filter) } |
            ForEach-Object { '{0,-12} {1} | {2}' -f $_.Current.ControlType.ProgrammaticName.Replace('ControlType.', ''), $_.Current.Name, $_.Current.AutomationId }
    }
}

# Health of the process as Task Manager would show it
function Get-TkHealth {
    $p = Get-TkProcess
    if (-not $p) { return [pscustomobject]@{ Alive = $false } }
    $p.Refresh()
    $hung = $false
    [TkUia]::EnumWindows({ param($h, $l) $id = 0; [void][TkUia]::GetWindowThreadProcessId($h, [ref]$id)
        if ($id -eq $p.Id -and [TkUia]::IsWindowVisible($h) -and [TkUia]::IsHungAppWindow($h)) { $script:hungSeen = $true }; $true }, [IntPtr]::Zero) | Out-Null
    if ($script:hungSeen) { $hung = $true; $script:hungSeen = $false }
    $kids = @(Get-CimInstance Win32_Process -Filter "ParentProcessId=$($p.Id)" -ErrorAction SilentlyContinue)
    [pscustomobject]@{
        Alive = $true; Pid = $p.Id; Hung = $hung; Responding = $p.Responding
        CpuSec = [math]::Round($p.TotalProcessorTime.TotalSeconds, 2)
        PrivateMB = [math]::Round($p.PrivateMemorySize64 / 1MB, 1)
        WorkingMB = [math]::Round($p.WorkingSet64 / 1MB, 1)
        Handles = $p.HandleCount; Threads = $p.Threads.Count; Children = $kids.Count
        GdiUser = 0
    }
}

# Quits through rc and reports what is left behind
function Stop-Tk([int]$WaitSec = 20) {
    $p = Get-TkProcess
    Send-Tk 'quit' | Out-Null
    $t0 = Get-Date
    while ((Get-Date) -lt $t0.AddSeconds($WaitSec)) {
        if (-not (Get-Process terrikon, vlc-preparser -ErrorAction SilentlyContinue | Where-Object { -not $_.HasExited })) { break }
        Start-Sleep -Milliseconds 250
    }
    $left = @(Get-Process terrikon, vlc-preparser -ErrorAction SilentlyContinue | Where-Object { -not $_.HasExited })
    [pscustomobject]@{ ExitSeconds = [math]::Round(((Get-Date) - $t0).TotalSeconds, 1); LeftOver = ($left | ForEach-Object { "$($_.ProcessName):$($_.Id)" }) -join ',' }
}

Export-ModuleMember -Function *-Tk*, Get-TkProcess, Test-TkRc
