# Which action leaks: each one repeated, memory and handles before and after
param([string[]]$Extra = @(), [int]$Times = 15, [string[]]$Tests = @('video', 'music', 'projectm', 'goom', 'spectrum', 'own', 'pip', 'mini'))
Import-Module E:\VLC-fork\scripts\tk.psm1 -Force
$music = 'file:///E:/VLC-fork/pipeline-test/music.mp3'
$video = 'file:///E:/VLC-fork/test-two-tracks.mkv'
function M { Start-Sleep 4; $h = Get-TkHealth; [pscustomobject]@{ MB = $h.PrivateMB; H = $h.Handles; T = $h.Threads } }
function Vis([string]$name) {
    if (-not (Invoke-Tk -Id audioVisualizationButton)) { Invoke-Tk -Name 'Визуализация' | Out-Null }
    Start-Sleep -Milliseconds 600; Invoke-Tk -Name $name -Type MenuItem | Out-Null; Start-Sleep 3
}
function OpenMusic { Send-Tk 'clear', "add $music" | Out-Null; Start-Sleep 2; Invoke-Tk -Id artworkInfoWidget | Out-Null; Start-Sleep 2 }

$results = foreach ($test in $Tests) {
    Stop-Tk | Out-Null; Start-Tk -ExtraArgs $Extra; & E:\VLC-fork\scripts\win.ps1 -Maximize | Out-Null; Start-Sleep 3
    switch ($test) { { $_ -in 'projectm', 'goom', 'spectrum', 'own' } { OpenMusic } 'pipsame' { Send-Tk 'clear', "add $video", 'repeat on' | Out-Null; Start-Sleep 3 } }
    # one round first: what is made once and kept is not a leak
    $round = {
        switch ($test) {
            'video'    { Send-Tk 'clear', "add $video" | Out-Null; Start-Sleep 4; Send-Tk 'stop' | Out-Null; Start-Sleep 1 }
            'music'    { OpenMusic; Invoke-Tk -Id topBarBackButton | Out-Null; Start-Sleep 1 }
            'projectm' { Vis 'VLC: Эффект libprojectM'; Vis 'VLC: отключить' }
            'goom'     { Vis 'VLC: Эффект Goom'; Vis 'VLC: отключить' }
            'spectrum' { Vis 'VLC: Спектр'; Vis 'VLC: отключить' }
            'own'      { Vis 'Волна'; Vis 'Кольцо вокруг обложки' }
            'pip'      { Send-Tk 'clear', "add $video" | Out-Null; Start-Sleep 2; & E:\VLC-fork\scripts\win.ps1 -Minimize | Out-Null; Start-Sleep 2; & E:\VLC-fork\scripts\win.ps1 -Maximize | Out-Null; Start-Sleep 1 }
            'pipsame'  { & E:\VLC-fork\scripts\win.ps1 -Minimize | Out-Null; Start-Sleep 2; & E:\VLC-fork\scripts\win.ps1 -Maximize | Out-Null; Start-Sleep 2 }
            'mini'     { Send-Tk 'clear', "add $music" | Out-Null; Start-Sleep 2; & E:\VLC-fork\scripts\win.ps1 -Minimize | Out-Null; Start-Sleep 2; & E:\VLC-fork\scripts\win.ps1 -Maximize | Out-Null; Start-Sleep 1 }
        }
    }
    & $round
    $a = M
    for ($i = 0; $i -lt $Times; $i++) { & $round }
    Send-Tk 'stop' | Out-Null
    $b = M
    $r = [pscustomobject]@{ Test = $test; MBperRound = [math]::Round(($b.MB - $a.MB) / $Times, 1); HandlesPerRound = [math]::Round(($b.H - $a.H) / $Times, 1); ThreadsPerRound = [math]::Round(($b.T - $a.T) / $Times, 2); From = $a.MB; To = $b.MB }
    Write-Host ($r | Out-String)
    $r
}
$results | Format-Table -AutoSize | Out-String | Set-Content E:\VLC-fork\pipeline-test\leak.txt -Encoding utf8
$results | Format-Table -AutoSize
