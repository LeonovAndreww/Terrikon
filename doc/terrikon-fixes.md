# Terrikon: bugs fixed compared with VLC 4

Terrikon is based on VLC 4 at upstream commit c6d1df3. This list keeps the
bugs that were already in VLC 4 there and are fixed in Terrikon, so they can
be told apart from our own (also listed, at the end).

How a bug is placed: the code it comes from is compared with c6d1df3
(`git diff c6d1df3 -- <file>`, `git blame`). Code we never touched means the
bug is upstream's; when unsure, the original build is run.

## Upstream bugs fixed

| Area | Bug in VLC 4 | Fix in Terrikon |
|------|--------------|-----------------|
| Player | After a seek, the time and the seek bar stand still for about a second while the playback goes on: the first point after a seek is sent without a date, and the next one only comes a second later | the interface goes on from the first point when playing |
| Media library | Media of unknown length show "-1 ms" in the lists | shown as "--:--" |
| Media library | Bookmarks model nests a reset inside a reset each time a media starts ("beginResetModel called ... without calling endResetModel first") | `mlbookmarkmodel.cpp`: no second refresh inside the reset |
| Interface | Service names (network, devices) not translated | `qtr` instead of `qfu` for module long names |
| Interface | Random visualization switching | visualization chosen from a menu |
| Interface | Hotkeys do nothing with a non-Latin keyboard layout | keys matched by their native scan code |
| Interface | About 1450 strings missing from the Russian translation | translated |
| Core | `spawn.c` lost the child's error output | kept |
| Core | Signals emitted while a lock is held | emitted after unlocking |
| Browse | A drive without a label (usually C:) has no name; labels in other languages come out garbled (ANSI call) | "Label (E:)", "Local disk (C:)", read with the wide call |
| Browse | On Windows the Documents folder is shown as "Home"; the Desktop folder shares its string with the screen capture, translated "Screen" | each folder keeps its own name |
| Tables | The sort arrow points up for Z to A, unlike the file explorer | up is A to Z; a new column starts from A to Z |
| Windows | Closing could leave the process running with no window, holding the files and the drive it had opened (seen after days): on the way out Windows ends the threads left over wherever they are, and one ended holding the lock of the thread keys left the C++ runtime clean up of the Qt module waiting forever | `bin/winvlc.c`: once libvlc is released, the process ends at once |
| Player | With "play and pause" (pause at the end), playing again stays at the end, and a seek leaves it paused | playing again starts over; a seek plays on |
| Player | The question to resume a video takes the place of the back button and the logo | shown beside them |
| Player | Leaving a stopped video showed the small in-window player at the bottom left for a moment | not shown while the video is closing |

## Our own bugs (introduced by Terrikon, fixed)

| Bug | Cause |
|-----|-------|
| Videos and the player failed to load (b89e9d3) | missing QML imports, a grouped property set on an instance |
| Blurry simple preferences icons | our SVG icons were scaled with VLC's pixmap code meant for its PNGs |
| Folder names "Скачать" (the verb) and "Главная" (the home page) in Browse | our out-of-context translations of "Download" and "Home" |
| "Skin" translated as "обложка" (cover) in the preferences and the menu | our translation; back to "оболочка" |
| Pink (magenta) tool tips over the folder chips on Home and the folder buttons on covers | tool tips placed directly without the palette VLC's own ones carry |
| Leaving the player while a visualization played stopped the music; once a grey view was left behind (not seen again since) | our leaving code stopped playback; now music goes on |
| Clip size estimate far below the result for 4K phone videos (7-15 MB shown, 63 MB made) | our rough guess capped the rate at 8 Mbit/s and was shown while the real measurement still ran |
| "Fit in N MB" made clips as big as the limit even when the best quality was smaller | the limit was used as the rate to aim at; now it only caps the rate |
| AV1 export failed with "Unknown encoder 'libsvtav1'" | the encoder was assumed; ffmpeg builds differ, the one present is used now |
| The mini window of the music came back on minimizing after being closed | closing only turned off the setting, not the automatic showing |
| Maximizing the picture in picture window (task bar, Win+Up) left the small player stretched over the screen, with no way back to the player | picture in picture did not follow the window state; maximizing now leaves it |
| "Fit in N MB" with AV1 came out some 15% over the limit | libaom misses the rate; an over-limit clip is now made once more at a lower rate |

## Known upstream issues, not fixed yet

- `ListViewExt.qml`: the footer's height depends on its own position ("Binding loop detected for property implicitHeight"). Harmless.
- `BrowseDeviceView.qml`: "Unable to assign [undefined] to QQmlComponent*" for sections without a header. Harmless.
- Changing the audio filters and the VLC visualization at the same moment can stall the audio output (playing, but the time stands still). Terrikon switches them one after the other.
- Each time a Direct3D11 video output closes, about 12 MB and 14 handles stay
  behind: switching a VLC visualization on and off, or every track played
  with one (the visualization gets an output of its own each time). The same
  in a VLC 4 build from before the Terrikon changes; none with `--vout=gl`
  or the dummy output, and none when creating and releasing a D3D11 device
  alone (`test/terrikon/d3dleak.cpp`). Terrikon's own visualizations are not
  concerned. Measured with `test/terrikon/leak-compare.ps1`.
- Ours, partly: picture in picture in and out leaves 5 MB (OpenGL output) to 15 MB
  (Direct3D11) each time; not looked into further yet.
