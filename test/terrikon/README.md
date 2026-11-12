# Terrikon tests on Windows

PowerShell 5.1 scripts that drive a built Terrikon without the mouse, through
Windows UI Automation (controls by their accessible name or id) and the rc
interface, and watch the process as Task Manager would: not responding windows,
memory, handles, threads, processes left after quitting, files still held.

They expect the layout used to build Terrikon here (`E:\VLC-fork\terrikon`,
test media in `E:\VLC-fork\pipeline-test`); change the paths at the top of each
script for another one. Tests run a copy of the installed build
(`E:\VLC-fork\terrikon-test`, refreshed by `Sync-TkCopy`) with a `portable`
folder next to it, so VLC keeps the settings, media library and history of
the tests there and leaves the user's alone (overriding `%APPDATA%` does not
do it: VLC asks Windows for the folder). `Start-Tk -RealProfile` runs the
installed build with the real profile.

The sandbox the scripts may run in neither sees the user's mapped network
drives nor waits on the network, and synthetic input (SendInput) does not
reach the player from it: network tests (`unc-hang.ps1`) run outside it.

- `tk.psm1`: `Start-Tk`, `Stop-Tk` (reports what is left running), `Send-Tk`
  (rc commands), `Find-Tk` / `Invoke-Tk` (find and press a control; `-Double`,
  `-Right` for real clicks), `Get-TkControls` (what can be pressed now),
  `Get-TkHealth`, `Send-TkMouse` (clicks as window messages), `Save-TkShot`.
- `soak.ps1 -Minutes 120`: music with every visualization and back, videos
  with seeks and pauses, the end of a file, picture in picture and the mini
  window, in a loop; a CSV of the health all along and a summary with the
  trends per hour.
- `leak.ps1`: each action repeated, memory and handles per round.
- `leak-compare.ps1`: the same loop in two builds, or with another video
  output (`-Vout gl`), to tell our leaks from VLC's.
- `unc-hang.ps1`: opens a media on `\\10.255.255.1`, which never answers,
  and counts the seconds the window does not respond (anything the
  interface thread does with that path shows up).
- `crashhunt.ps1`: starts, plays, idles past the 30 s after which Qt's pool
  threads end, quits, in a loop; crashes leave minidumps in
  `%LOCALAPPDATA%\terrikon\crashes`.
- `d3dleak.cpp`: creates and releases a D3D11 device in a loop (the driver
  alone does not leak).

The clip export is tested inside the player: `TERRIKON_SELFTEST=<config.json>`
makes it export the clips listed there and write `<config>.result.json`, with
the size shown beforehand next to the size made.

## Driving the interface (automation)

Started with `TERRIKON_AUTOMATION=<port>` in the environment (Start-Tk uses
4213), Terrikon listens on 127.0.0.1 for one command per line and answers
one line of JSON: `items`, `at X Y`, `hover`, `click`, `wheel`, `key`, `type`,
`get`/`set`/`props`, `eval`, `grab`, `player`, `widgets`, `windows`; `help`
lists them. `Send-TkUi` in tk.psm1 sends them. Pointer, clicks and keys go to
Qt itself, so hovering works without taking the desktop's mouse.
