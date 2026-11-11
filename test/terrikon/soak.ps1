# Long test of Terrikon: scenarios in a loop, health sampled all along,
# a summary at the end. Usage: soak.ps1 [-Minutes 60] [-Scenarios music,video,eof,window]
param([int]$Minutes = 30, [string[]]$Scenarios = @('music', 'video', 'eof', 'window'), [switch]$KeepRunning)
Import-Module E:\VLC-fork\scripts\tk.psm1 -Force
$ErrorActionPreference = 'Continue'

$dir = "E:\VLC-fork\pipeline-test\soak\$(Get-Date -Format yyyyMMdd-HHmmss)"
New-Item -ItemType Directory -Force $dir | Out-Null
$metrics = "$dir\metrics.csv"; $events = "$dir\events.log"
$problems = New-Object System.Collections.ArrayList

function Note([string]$text) { $line = "$(Get-Date -Format HH:mm:ss) $text"; Add-Content $events $line -Encoding utf8; Write-Host $line }
function Problem([string]$text) { [void]$problems.Add("$(Get-Date -Format HH:mm:ss) $text"); Note "PROBLEM: $text" }
function Sample([string]$step) {
    $h = Get-TkHealth
    $row = [pscustomobject]@{ Time = (Get-Date -Format o); Step = $step; Alive = $h.Alive; Hung = $h.Hung
        CpuSec = $h.CpuSec; PrivateMB = $h.PrivateMB; WorkingMB = $h.WorkingMB; Handles = $h.Handles
        Threads = $h.Threads; Children = $h.Children }
    $row | Export-Csv $metrics -Append -NoTypeInformation -Encoding utf8
    if (-not $h.Alive) { Problem "process gone during '$step'" }
    elseif ($h.Hung) { Problem "window not responding during '$step'" }
    $h
}
# Waits up to $sec for the window to answer again; a hang longer than that is a problem
function WaitResponsive([string]$step, [int]$sec = 5) {
    $t0 = Get-Date
    while ((Get-Date) -lt $t0.AddSeconds($sec)) { $h = Get-TkHealth; if ($h.Alive -and -not $h.Hung) { return $true }; Start-Sleep -Milliseconds 250 }
    Problem "not responding for more than $sec s after '$step'"; $false
}
# The playing time moves on (catches "time runs without sound" only partly: a
# stalled output stops the clock in VLC 4)
function CheckTimeMoves([string]$step, [int]$sec = 3) {
    $a = Get-TkTime; Start-Sleep -Seconds $sec; $b = Get-TkTime
    if ($null -eq $a -or $null -eq $b) { Problem "rc gave no time after '$step'"; return }
    if ($b -le $a) { Problem "time stands still after '$step' ($a -> $b)" }
}

$music = 'file:///E:/VLC-fork/pipeline-test/music.mp3'
$videos = @('file:///E:/VLC-fork/test-two-tracks.mkv', 'file:///E:/VLC-fork/pipeline-test/source.mkv',
            'file:///E:/VLC-fork/pipeline-test/chapters.mkv', 'file:///E:/VLC-fork/pipeline-test/vertical.mp4')
$short = 'file:///E:/VLC-fork/pipeline-test/source_clip_00-00-05.mp4'

function Scenario-Music {
    Send-Tk 'clear', "add $music" | Out-Null; Start-Sleep 2
    Invoke-Tk -Id artworkInfoWidget | Out-Null; Start-Sleep 2
    if (-not (Find-Tk -Id audioVisualizationButton -TimeoutMs 4000)) { Problem 'music player did not open (no visualization button)' }
    Sample 'music: player' | Out-Null
    $items = @('Кольцо вокруг обложки', 'Волна', 'VLC: Спектр', 'Столбики', 'VLC: Эффект Goom', 'Линия',
               'VLC: Эффект libprojectM', 'VLC: Визуализация спектра в 3D (через OpenGL)', 'Холмы', 'VLC: отключить')
    foreach ($item in $items) {
        if (-not (Invoke-Tk -Id audioVisualizationButton)) { Invoke-Tk -Name 'Визуализация' | Out-Null }
        Start-Sleep -Milliseconds 600
        if (-not (Invoke-Tk -Name $item -Type MenuItem)) { Problem "menu item missing: $item"; Send-Tk 'key key-nav-cancel' | Out-Null; continue }
        Start-Sleep 2
        WaitResponsive "visualization $item" | Out-Null
        CheckTimeMoves "visualization $item" 2
        Sample "music: $item" | Out-Null
    }
    Invoke-Tk -Id topBarBackButton | Out-Null
    WaitResponsive 'back from music' 5 | Out-Null
    CheckTimeMoves 'back from music' 2
    Sample 'music: back' | Out-Null
}

function Scenario-Video {
    Send-Tk 'clear' | Out-Null
    foreach ($v in $videos) { Send-Tk "enqueue $v" | Out-Null }
    Send-Tk 'play' | Out-Null; Start-Sleep 3
    for ($i = 0; $i -lt $videos.Count; $i++) {
        Sample "video $i start" | Out-Null
        foreach ($s in 1..5) {
            Send-Tk "seek $(Get-Random -Minimum 0 -Maximum 60)%" | Out-Null
            Start-Sleep -Milliseconds 800
        }
        CheckTimeMoves "video $i seeks" 2
        Send-Tk 'pause' | Out-Null; Start-Sleep 1; Send-Tk 'pause' | Out-Null
        CheckTimeMoves "video $i resume" 2
        Send-Tk 'next' | Out-Null; Start-Sleep 2
        WaitResponsive "video $i next" | Out-Null
    }
    Send-Tk 'stop' | Out-Null; Start-Sleep 1
    Invoke-Tk -Id topBarBackButton | Out-Null
    WaitResponsive 'back from video' | Out-Null
    Sample 'video: done' | Out-Null
}

# At the end with pause-at-end: play again starts over, a seek plays on
function Scenario-Eof {
    Send-Tk 'clear', "add $short" | Out-Null
    Start-Sleep 8
    $t = Get-TkTime
    $st = Send-Tk 'status'
    if ($st -notmatch 'state paused|pause state|state stopped|play state: 4') { Note "status at end: $($st -replace '\s+', ' ')" }
    Send-Tk 'pause' | Out-Null; Start-Sleep 2
    $t2 = Get-TkTime
    if ($null -ne $t2 -and $t2 -gt 3) { Problem "play after the end did not start over (time $t2)" }
    Start-Sleep 5
    Send-Tk 'seek 1' | Out-Null; Start-Sleep 1
    CheckTimeMoves 'seek after the end' 2
    Sample 'eof' | Out-Null
}

function Scenario-Window {
    Send-Tk 'clear', "add $($videos[0])" | Out-Null; Start-Sleep 3
    & E:\VLC-fork\scripts\win.ps1 -Minimize | Out-Null; Start-Sleep 2
    WaitResponsive 'minimize with video (PiP)' | Out-Null; Sample 'pip' | Out-Null
    & E:\VLC-fork\scripts\win.ps1 -Maximize | Out-Null; Start-Sleep 2
    WaitResponsive 'restore from PiP' | Out-Null
    if (-not (Find-Tk -Name 'Назад' -Type Button -TimeoutMs 3000)) { Problem 'still in picture in picture after maximizing (no back button)' }
    Send-Tk 'clear', "add $music" | Out-Null; Start-Sleep 2
    & E:\VLC-fork\scripts\win.ps1 -Minimize | Out-Null; Start-Sleep 2
    WaitResponsive 'minimize with music (mini window)' | Out-Null; Sample 'mini' | Out-Null
    & E:\VLC-fork\scripts\win.ps1 -Maximize | Out-Null; Start-Sleep 2
    WaitResponsive 'restore from mini' | Out-Null
}

if (-not (Get-TkProcess)) { Start-Tk; Start-Sleep 3 }
& E:\VLC-fork\scripts\win.ps1 -Maximize | Out-Null
Note "soak for $Minutes min: $($Scenarios -join ',')"
$first = Sample 'start'
$end = (Get-Date).AddMinutes($Minutes); $round = 0
while ((Get-Date) -lt $end) {
    $round++
    foreach ($s in $Scenarios) {
        Note "round ${round}: $s"
        try { & "Scenario-$s" } catch { Problem "scenario $s threw: $_" }
        if (-not (Get-TkProcess)) { Problem 'process gone, restarting'; Start-Tk; Start-Sleep 3 }
        if ((Get-Date) -ge $end) { break }
    }
}
$last = Sample 'end'

$exit = $null
if (-not $KeepRunning) {
    $exit = Stop-Tk 30
    if ($exit.LeftOver) { Problem "left running after quit: $($exit.LeftOver)" }
    if ($exit.ExitSeconds -gt 10) { Problem "quitting took $($exit.ExitSeconds) s" }
    try { $f = [IO.File]::Open('E:\VLC-fork\pipeline-test\music.mp3', 'Open', 'ReadWrite', 'None'); $f.Close() }
    catch { Problem 'a played file is still held after quit' }
}

# Trends: growth per hour of memory and handles, from a straight line fit
$rows = Import-Csv $metrics | Where-Object { $_.Alive -eq 'True' }
function Slope($field) {
    $t0 = [datetime]$rows[0].Time
    $xs = $rows | ForEach-Object { (([datetime]$_.Time) - $t0).TotalHours }
    $ys = $rows | ForEach-Object { [double]($_.$field -replace ',', '.') }
    $n = $xs.Count; if ($n -lt 3) { return 0 }
    $mx = ($xs | Measure-Object -Average).Average; $my = ($ys | Measure-Object -Average).Average
    $num = 0; $den = 0
    for ($i = 0; $i -lt $n; $i++) { $num += ($xs[$i] - $mx) * ($ys[$i] - $my); $den += ($xs[$i] - $mx) * ($xs[$i] - $mx) }
    if ($den -eq 0) { 0 } else { [math]::Round($num / $den, 1) }
}
$hours = (([datetime]$rows[-1].Time) - ([datetime]$rows[0].Time)).TotalHours
$cpu = if ($hours -gt 0) { [math]::Round(([double]($rows[-1].CpuSec -replace ',', '.') - [double]($rows[0].CpuSec -replace ',', '.')) / ($hours * 3600) * 100 / [Environment]::ProcessorCount, 1) } else { 0 }
$summary = @"
Terrikon soak $(Get-Date -Format 'yyyy-MM-dd HH:mm'), $Minutes min, $round rounds of $($Scenarios -join ', ')
Memory (private): $($first.PrivateMB) -> $($last.PrivateMB) MB, trend $(Slope 'PrivateMB') MB/h, max $(($rows | % { [double]($_.PrivateMB -replace ',', '.') } | Measure-Object -Maximum).Maximum) MB
Handles: $($first.Handles) -> $($last.Handles), trend $(Slope 'Handles')/h
Threads: $($first.Threads) -> $($last.Threads), trend $(Slope 'Threads')/h
Average CPU (all cores): $cpu %
Quit: $(if ($exit) { "$($exit.ExitSeconds) s, left over: $(if ($exit.LeftOver) { $exit.LeftOver } else { 'nothing' })" } else { 'kept running' })
Problems: $($problems.Count)
$($problems -join "`n")
"@
$summary | Set-Content "$dir\summary.txt" -Encoding utf8
$summary
