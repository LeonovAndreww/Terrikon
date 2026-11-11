# The same loop in two builds: music with VLC's spectrum, play and stop
param([int]$Times = 12, [string]$Visual = 'spectrum', [string]$Vout = '', [switch]$OnlyTerrikon)
Import-Module E:\VLC-fork\scripts\tk.psm1 -Force
$music = 'file:///E:/VLC-fork/pipeline-test/music.mp3'
$builds = @(@{ Name = 'terrikon'; Exe = 'E:\VLC-fork\terrikon\terrikon.exe'; Proc = 'terrikon' },
            @{ Name = 'early vlc 4 (03.11)'; Exe = 'E:\VLC-fork\vlc-4.0.0-dev\vlc.exe'; Proc = 'vlc' })
foreach ($b in $builds) {
    if ($OnlyTerrikon -and $b.Name -ne 'terrikon') { continue }
    Get-Process terrikon, vlc -ErrorAction SilentlyContinue | ForEach-Object { Send-Tk 'quit' | Out-Null; Start-Sleep 3 }
    $saved = $env:APPDATA; $env:APPDATA = 'E:\VLC-fork\test-profile'
    $args2 = @('--extraintf=rc', '--rc-host=127.0.0.1:4212', '--rc-quiet', '--audio-visual=visual', "--effect-list=$Visual", '--no-qt-video-autoresize')
    if ($Vout) { $args2 += "--vout=$Vout" }
    Start-Process $b.Exe -WorkingDirectory (Split-Path $b.Exe) -ArgumentList $args2 | Out-Null
    $env:APPDATA = $saved
    for ($i = 0; $i -lt 40 -and -not (Test-TkRc); $i++) { Start-Sleep -Milliseconds 500 }
    $p = Get-Process $b.Proc | Sort StartTime | Select -Last 1
    $round = { Send-Tk 'clear', "add $music" | Out-Null; Start-Sleep 4; Send-Tk 'stop' | Out-Null; Start-Sleep 2 }
    & $round; & $round
    Start-Sleep 3; $p.Refresh(); $m0 = $p.PrivateMemorySize64 / 1MB; $h0 = $p.HandleCount; $t0 = $p.Threads.Count
    for ($i = 0; $i -lt $Times; $i++) { & $round }
    Start-Sleep 3; $p.Refresh()
    [pscustomobject]@{ Build = $b.Name; Visual = $Visual; Vout = $Vout; MBperRound = [math]::Round(($p.PrivateMemorySize64 / 1MB - $m0) / $Times, 1)
        HandlesPerRound = [math]::Round(($p.HandleCount - $h0) / $Times, 1); ThreadsPerRound = [math]::Round(($p.Threads.Count - $t0) / $Times, 2) }
    Send-Tk 'quit' | Out-Null; Start-Sleep 4
}
