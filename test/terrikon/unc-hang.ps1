param([string]$Label)
Import-Module E:\VLC-fork\scripts\tk.psm1 -Force
Start-Tk | Out-Null; Start-Sleep 4
$p = Get-TkProcess
Send-Tk 'add file://10.255.255.1/share/test.wav' | Out-Null
$bad=0; for ($i=0; $i -lt 25; $i++) { Start-Sleep 1; $p.Refresh(); if (-not $p.Responding) { $bad++ } }
"$Label : not responding $bad of 25"
Stop-Process -Id $p.Id -Force