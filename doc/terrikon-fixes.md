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
| Video output | Each close of a Direct3D11 output with a software decoded video or a visualization lost the whole D3D11 device, some 12 MB and 14 handles: the staging textures were overwritten, not released, when the format resources were made again | `direct3d11.cpp`: the old staging textures are released first (found with the D3D11 debug layer) |

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
| Clip export wrote straight into the output: a failed or cancelled export left a broken or empty file under the final name; "Save as" over an existing file, or over the video itself, reported success and wrote nothing | ffmpeg writes a part file next to the output, renamed only when complete (an existing file is set aside and put back on failure); the source itself is refused |
| A copied clip of VP8/Vorbis and other streams MP4 does not take failed | ffprobe tells the codecs; Matroska then |
| The clip could take a cover picture for the video | the first real video stream (`0:V:0`) |
| Vertical clip of a source narrower than 9:16 failed, anamorphic sources came out with the wrong shape | the 9:16 window fits in the frame; square pixels first |
| Waveform, thumbnails and size measuring waited forever when ffmpeg could not start | a start failure ends them as a failure |
| The player stopped responding at start (for a minute or more) when a media library folder was on a network drive that did not answer: the folder suggestions of the home page looked at every library folder on disk, on the interface thread | the suggestions are worked out on a thread of their own; library folders are compared by name |
| The player stopped responding while the current media was on a network drive that did not answer: the seek bar's thumbnails and waveform and the moment marks looked up the file's size and date on the interface thread | looked up on a thread of their own (`offThread`); measured with a media on an unreachable `\\10.255.255.1` path: 21 of 25 s not responding before, none after |
| Random crashes (some a dozen a day), often at start or quit: a Qt thread pool thread that had used a regular expression ended (after 30 s idle) and Qt's per-thread PCRE2 JIT stack, a `thread_local`, crashed in its destructor under MinGW's emulated TLS | regular expressions without JIT (`QT_ENABLE_REGEXP_JIT=0`); crashes now leave a minidump in `%LOCALAPPDATA%\terrikon\crashes` |
| The next episode's opening got no "Skip opening" button when it was the same chapter number as in the last one | the countdown starts again with each media |
| A clip range could shrink under its minimum length when the media turned out shorter | it starts earlier instead |
| Playing a folder from Videos queued its audio files too | videos only |
| The mini window was placed at the default spot when left on a screen left of the main one, and off screen when its screen was gone | kept where it was if that is on a screen, else the bottom right of the main one |
| Minimizing and restoring quickly could still send the video to picture in picture | the state is checked again when it happens |
| Clip export could take another audio track than the one ticked when the player lists them in another order than the file | the track's place among the file's audio streams |
| The Videos folder tree was built again for the count, each page and each lookup | built once, again after a library change |
| A folder dropped (or picked) that is already in the library, inside one of its folders, did nothing visible | Videos opens on it |
| Picture in picture lost 5 to 15 MB each time in and out | it was the Direct3D11 staging leak (the window changes size, the output format resources are made again); now under 1 MB per round, as noise (`leak.ps1 -Tests pipsame,pip`) |
| The radio stations (and any directory of a network service) showed grey folders instead of their country flags and station logos in the grid view: the grid gave Qt the http(s) address directly, which it does not fetch, while the list view goes through VLC's access | the grid view too goes through `VLCAccessImage` (upstream VLC 4 bug) |
| Opening a video while a VLC visualization (Goom, spectrum...) was on for music: the visualization kept the player's window and the video opened in a window of its own | an input with a video gets no visualization on its sound (upstream VLC 4 behaviour) |
| Muting a preview in the library muted Terrikon as a whole, in the Windows volume mixer, from run to run | the preview plays in an audio session of its own (Terrikon's own bug) |

## Known upstream issues, not fixed yet

- `ListViewExt.qml`: the footer's height depends on its own position ("Binding loop detected for property implicitHeight"). Harmless.
- `BrowseDeviceView.qml`: "Unable to assign [undefined] to QQmlComponent*" for sections without a header. Harmless.
- Changing the audio filters and the VLC visualization at the same moment can stall the audio output (playing, but the time stands still). Terrikon switches them one after the other.
