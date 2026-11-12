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

# Tests run a copy of the installed build with a "portable" folder next to
# it: VLC then keeps its settings, media library and history there, apart
# from the user's (an APPDATA override does not work: VLC asks Windows for
# the folder). Sync-TkCopy refreshes the copy after an install.
$script:Installed = 'E:\VLC-fork\terrikon'
$script:TestDir = 'E:\VLC-fork\terrikon-test'
$script:Exe = "$script:TestDir\terrikon.exe"
function Sync-TkCopy {
    robocopy $script:Installed $script:TestDir /MIR /XD portable /NFL /NDL /NJH /NJS /NP | Out-Null
    New-Item -ItemType Directory -Force "$script:TestDir\portable" | Out-Null
}
$script:Log = 'E:\VLC-fork\pipeline-test\ui-log.txt'

function Get-TkProcess { Get-Process terrikon -ErrorAction SilentlyContinue | Where-Object { -not $_.HasExited } | Sort-Object StartTime | Select-Object -Last 1 }

function Start-Tk([string]$File, [switch]$RealProfile, [string[]]$ExtraArgs = @()) {
    Remove-Item $script:Log -ErrorAction SilentlyContinue
    # A profile of its own (settings, media library, history), so tests leave
    # the user's alone
    $exe = if ($RealProfile) { "$script:Installed\terrikon.exe" } else { if (-not (Test-Path $script:Exe)) { Sync-TkCopy }; $script:Exe }
    # the interface driven by Send-TkUi (terrikon/automation.cpp)
    $env:TERRIKON_AUTOMATION = '4213'
    Start-Process $exe -WorkingDirectory (Split-Path $exe) -ArgumentList (@(
        '--no-qt-video-autoresize', '--extraintf=rc', '--rc-host=127.0.0.1:4212', '--rc-quiet',
        '--file-logging', '--log-verbose=1', "--logfile=$script:Log") + $ExtraArgs) | Out-Null
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

# Drives the interface itself (TERRIKON_AUTOMATION): one command per line,
# one JSON object back each (send 'help' for the list). The pointer, the
# clicks and the keys go to Qt, not to the desktop: nothing is borrowed
# from the user, and hovering works in a window behind the others.
$script:Ui = $null
function Send-TkUi([string[]]$Commands, [switch]$Raw) {
    $answers = foreach ($cmd in $Commands) {
        for ($try = 0; $try -lt 2; $try++) {
            try {
                if (-not $script:Ui -or -not $script:Ui.Connected) {
                    $c = New-Object Net.Sockets.TcpClient; $c.Connect('127.0.0.1', 4213)
                    $c.ReceiveTimeout = 15000
                    $s = $c.GetStream()
                    $script:Ui = $c
                    $script:UiW = New-Object IO.StreamWriter($s, (New-Object Text.UTF8Encoding $false)); $script:UiW.AutoFlush = $true
                    $script:UiR = New-Object IO.StreamReader($s, [Text.Encoding]::UTF8)
                }
                $script:UiW.WriteLine($cmd)
                $line = $script:UiR.ReadLine()
                if ($null -eq $line) { throw 'closed' }
                if ($Raw) { $line } else { $line | ConvertFrom-Json }
                break
            } catch { $script:Ui = $null }
        }
    }
    $answers
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


# Mouse clicks sent as window messages to the window under the point (input
# injection with SendInput is dropped on this machine); the cursor itself is
# moved for real, for what reads its position (autoscroll, hover).
if (-not ('TkMsg' -as [type])) {
Add-Type @"
using System; using System.Runtime.InteropServices;
public static class TkMsg {
  [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] static extern IntPtr WindowFromPoint(POINT p);
  [DllImport("user32.dll")] static extern bool ScreenToClient(IntPtr h, ref POINT p);
  [DllImport("user32.dll")] static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] static extern bool GetCursorPos(out POINT p);
  public static void Send(uint msg, uint keys) {
    POINT s; GetCursorPos(out s); IntPtr h = WindowFromPoint(s); POINT c = s; ScreenToClient(h, ref c);
    PostMessage(h, msg, (IntPtr)keys, (IntPtr)((c.Y << 16) | (c.X & 0xffff)));
  }
}
"@
}
function Send-TkMouse([int]$X = -1, [int]$Y = -1, [ValidateSet('move', 'left', 'right', 'middle', 'leftdown', 'leftup', 'middledown', 'middleup')][string]$Action = 'move') {
    if ($X -ge 0) { [TkMsg]::SetCursorPos($X, $Y) | Out-Null; Start-Sleep -Milliseconds 60; [TkMsg]::Send(0x0200, 0) }
    # message, key state while down
    $m = @{ leftdown = @(0x0201, 1); leftup = @(0x0202, 0); rightdown = @(0x0204, 2); rightup = @(0x0205, 0); middledown = @(0x0207, 0x10); middleup = @(0x0208, 0) }
    switch ($Action) {
        'move' { }
        { $_ -in 'left', 'right', 'middle' } { $d = $m["${_}down"]; $u = $m["${_}up"]; [TkMsg]::Send($d[0], $d[1]); Start-Sleep -Milliseconds 60; [TkMsg]::Send($u[0], $u[1]) }
        default { [TkMsg]::Send($m[$Action][0], $m[$Action][1]) }
    }
}
# A screenshot of the screen, scaled down (cheap to look at)
function Save-TkShot([string]$Path, [double]$Scale = 0.25, [int[]]$Rect) {
    Add-Type -AssemblyName System.Drawing
    $b = New-Object Drawing.Bitmap 1920, 1080; $g = [Drawing.Graphics]::FromImage($b); $g.CopyFromScreen(0, 0, 0, 0, $b.Size)
    if ($Rect) { $b = $b.Clone([Drawing.Rectangle]::new($Rect[0], $Rect[1], $Rect[2], $Rect[3]), $b.PixelFormat) }
    (New-Object Drawing.Bitmap $b, ([int]($b.Width * $Scale)), ([int]($b.Height * $Scale))).Save($Path)
}
Export-ModuleMember -Function *-Tk*, Get-TkProcess, Test-TkRc, Send-TkMouse, Save-TkShot
