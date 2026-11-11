# Terrikon tests on Windows

PowerShell 5.1 scripts that drive a built Terrikon without the mouse, through
Windows UI Automation (controls by their accessible name or id) and the rc
interface, and watch the process as Task Manager would: not responding windows,
memory, handles, threads, processes left after quitting, files still held.

They expect the layout used to build Terrikon here (`E:\VLC-fork\terrikon`,
test media in `E:\VLC-fork\pipeline-test`); change the paths at the top of each
script for another one. Tests run with a profile of their own
(`E:\VLC-fork\test-profile`), so the user's settings, media library and
history are left alone; `Start-Tk -RealProfile` uses the real one.

- `tk.psm1`: `Start-Tk`, `Stop-Tk` (reports what is left running), `Send-Tk`
  (rc commands), `Find-Tk` / `Invoke-Tk` (find and press a control; `-Double`,
  `-Right` for real clicks), `Get-TkControls` (what can be pressed now),
  `Get-TkHealth`.
- `soak.ps1 -Minutes 120`: music with every visualization and back, videos
  with seeks and pauses, the end of a file, picture in picture and the mini
  window, in a loop; a CSV of the health all along and a summary with the
  trends per hour.
- `leak.ps1`: each action repeated, memory and handles per round.
- `leak-compare.ps1`: the same loop in two builds, or with another video
  output (`-Vout gl`), to tell our leaks from VLC's.
- `d3dleak.cpp`: creates and releases a D3D11 device in a loop (the driver
  alone does not leak).

The clip export is tested inside the player: `TERRIKON_SELFTEST=<config.json>`
makes it export the clips listed there and write `<config>.result.json`, with
the size shown beforehand next to the size made.
