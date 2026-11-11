Import-Module E:\VLC-fork\scripts\tk.psm1 -Force
$before = @(Get-ChildItem "$env:LOCALAPPDATA\terrikon\crashes" -EA SilentlyContinue).Count
for ($i=0; $i -lt 12; $i++) {
  Start-Tk -ExtraArgs @('--qt-start-minimized') | Out-Null
  Send-Tk 'add file:///E:/VLC-fork/test-two-tracks.mkv' | Out-Null; Start-Sleep 6
  Send-Tk 'stop' | Out-Null; Start-Sleep 35
  Send-Tk 'add file:///E:/VLC-fork/pipeline-test/music.mp3' | Out-Null; Start-Sleep 5
  Stop-Tk | Out-Null; Start-Sleep 2
}
$after = @(Get-ChildItem "$env:LOCALAPPDATA\terrikon\crashes" -EA SilentlyContinue).Count
"dumps: $before -> $after"
Get-WinEvent -FilterHashtable @{LogName='Application'; Id=1000; StartTime=(Get-Date).AddMinutes(-15)} -EA SilentlyContinue | ? { $_.Message -match 'terrikon' } | % { $_.TimeCreated }