# Terrikon: bugs fixed compared with VLC 4

Terrikon is based on VLC 4 at upstream commit c6d1df3. The bugs fixed fall
in three groups:

1. **VLC 4's own**: in VLC 4's code and seen in VLC 4 as it is.
2. **VLC 4's, brought out by Terrikon**: in VLC 4's code (or its toolchain),
   but seen only once Terrikon used that code in a way VLC 4 does not.
3. **Terrikon's own**: in code Terrikon wrote.

How a bug is placed: the code it comes from is compared with c6d1df3
(`git diff c6d1df3 -- <file>`, `git blame`). Code we never touched means the
bug is upstream's; whether VLC 4 shows it is then checked with the original
build, or by reading when the path is taken.

## 1. VLC 4's own bugs

| Area | Bug in VLC 4 | Fix in Terrikon |
|------|--------------|-----------------|
| Player | After a seek, the time and the seek bar stand still for about a second while the playback goes on: the first point after a seek is sent without a date, and the next one only comes a second later | the interface goes on from the first point when playing |
| Player | With "play and pause" (pause at the end), playing again stays at the end, and a seek leaves it paused | playing again starts over; a seek plays on |
| Player | The question to resume a video takes the place of the back button and the logo | shown beside them |
| Player | Leaving a stopped video showed the small in-window player at the bottom left for a moment | not shown while the video is closing |
| Player | Opening a video while a VLC visualization (Goom, spectrum...) is on for music: the visualization keeps the player's window and the video opens in a window of its own | an input with a video gets no visualization on its sound |
| Media library | Media of unknown length show "-1 ms" in the lists | shown as "--:--" |
| Media library | Bookmarks model nests a reset inside a reset each time a media starts ("beginResetModel called ... without calling endResetModel first") | `mlbookmarkmodel.cpp`: no second refresh inside the reset |
| Interface | Service names (network, devices) not translated | `qtr` instead of `qfu` for module long names |
| Interface | Random visualization switching | visualization chosen from a menu |
| Interface | Hotkeys do nothing with a non-Latin keyboard layout | keys matched by their native scan code |
| Interface | About 1450 strings missing from the Russian translation | translated |
| Interface | Every list without a sort chosen ran from Z to A: the default order was descending, and each page saved it as soon as it was left | ascending by default; the saved default is forgotten once |
| Browse | A drive without a label (usually C:) has no name; labels in other languages come out garbled (ANSI call) | "Label (E:)", "Local disk (C:)", read with the wide call |
| Browse | On Windows the Documents folder is shown as "Home"; the Desktop folder shares its string with the screen capture, translated "Screen" | each folder keeps its own name |
| Browse | Names sorted by code points: other alphabets out of order, "Episode 10" before "Episode 2" | compared in the language's alphabet, numbers as numbers |
| Browse | The sort of devices and services compared with `<=` and `>=`, not a strict order as `std::sort` needs (it may then read out of bounds on equal names) | strict comparisons |
| Browse | A network service (the radio stations...) said "Nothing to see here, go back" for seconds before its items came: it was taken as loaded at once | loading until the first item comes, 10 s at most |
| Browse | The radio stations (and any directory of a network service) showed grey folders instead of their country flags and station logos in the grid view: the grid gave Qt the http(s) address directly, which it does not fetch, while the list view goes through VLC's access | the grid view too goes through `VLCAccessImage` |
| Tables | The sort arrow points up for Z to A, unlike the file explorer | up is A to Z; a new column starts from A to Z |
| Radio | Some stations showed three errors "DVDRead could not open the disc """ each time they played: the "iso" demuxer of dvdread (priority 6, before avformat) was probed on the http stream and opened the default disc | probing a stream without a local file does nothing, and a failed probe shows no error, as dvdnav does |
| Core | `spawn.c` lost the child's error output | kept |
| Core | Signals emitted while a lock is held | emitted after unlocking |
| Core | With one VLC running, a second one hung at its start: its pipes were named `AnonPipe_<count>`, the same in every process, so it could not make them, nor start its preparser processes or the rc interface | `src/win32/filesystem.c`: the name holds the process id |
| Windows | Closing could leave the process running with no window, holding the files and the drive it had opened (seen after days): on the way out Windows ends the threads left over wherever they are, and one ended holding the lock of the thread keys left the C++ runtime clean up of the Qt module waiting forever | `bin/winvlc.c`: once libvlc is released, the process ends at once |
| Video output | Each close of a Direct3D11 output with a software decoded video or a visualization lost the whole D3D11 device, some 12 MB and 14 handles: the staging textures were overwritten, not released, when the format resources were made again | `direct3d11.cpp`: the old staging textures are released first (found with the D3D11 debug layer) |
| Services | Free Music Charts showed an empty page: its list on archive.org is gone (403) | no longer shipped |
| Services | The podcasts page, empty until a feed is added, said "Nothing to see here, go back"; the dialog to add one was only behind a right click on the service | it says how, and its button opens the dialog |
| Services | Clicking a service: "Cannot call method 'forceActiveFocus' of undefined" (the grid has no current item) | the call is skipped then |

## 2. VLC 4's bugs brought out by Terrikon

| Bug seen | VLC 4's cause | What brought it out | Fix |
|----------|---------------|---------------------|-----|
| Picture in picture lost 5 to 15 MB each time in and out | the Direct3D11 staging leak above | picture in picture changes the window's size, and the output makes its format resources again each time | the leak fixed; now under 1 MB per round, as noise (`leak.ps1 -Tests pipsame,pip`) |
| A file on a network drive began to play only once 128 MiB were read (27 s against 2.4 s, an OBS recording on an SMB share) | the prefetch filter read the whole free space of its buffer in one read, and the first bytes and the seek to the index at the end waited for it (`prefetch-read-size` is gone since 4.0) | Terrikon plays network drive files through the prefetch filter with a large buffer; VLC 4 does not use it by default | reads of 512 KiB at most: 0.7 s to start, a seek to the middle in 1.3 s |
| Random crashes (some a dozen a day), often at start or quit | Qt's per-thread PCRE2 JIT stack, a `thread_local`, crashed in its destructor under MinGW's emulated TLS when a thread pool thread that had used a regular expression ended (after 30 s idle) | Terrikon's own code used regular expressions on Qt's thread pool | regular expressions without JIT (`QT_ENABLE_REGEXP_JIT=0`); crashes now leave a minidump in `%LOCALAPPDATA%\terrikon\crashes` |
| "Cannot assign to non-existent property parentId" (and title) on opening a folder of Videos | the page loader gave the last page's properties to every page on the way | Home opens a folder of Videos directly; in VLC 4 the pages on the way are always loaded already | the pages on the way take only the properties they have |

## 3. Terrikon's own bugs

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
| The next episode's opening got no "Skip opening" button when it was the same chapter number as in the last one | the countdown starts again with each media |
| A clip range could shrink under its minimum length when the media turned out shorter | it starts earlier instead |
| Playing a folder from Videos queued its audio files too | videos only |
| The mini window was placed at the default spot when left on a screen left of the main one, and off screen when its screen was gone | kept where it was if that is on a screen, else the bottom right of the main one |
| Minimizing and restoring quickly could still send the video to picture in picture | the state is checked again when it happens |
| Clip export could take another audio track than the one ticked when the player lists them in another order than the file | the track's place among the file's audio streams |
| The Videos folder tree was built again for the count, each page and each lookup | built once, again after a library change |
| A folder dropped (or picked) that is already in the library, inside one of its folders, did nothing visible | Videos opens on it |
| Muting a preview in the library muted Terrikon as a whole, in the Windows volume mixer, from run to run | the preview plays in an audio session of its own |
| The player of a radio showed neither the seek bar nor "On air" | the "On air" row was never placed in the layout of the bar |
| "Binding loop detected for property active" in the video tiles | the preview's loader stayed active while its own item faded out; a flag of the tile now tells it |
| A preview in the library opened its video in windows of its own ("VLC Video Output", flashing white) | the preview's player made a window; it now draws only into memory (`window=wdummy`) |
| A folder chip of Home did nothing on a click, and "Open folder" showed "This PC" | the media library gives folders as `file:///c:%5CUsers%5C...`, which we passed on as they were; a usual file URL is now made of the path |
| What an opened folder holds came after every folder, apart from its place | it was sorted with all the rest; it now keeps the folder's place |
| The tiles of an opened folder came in with an animation each time one was made again while scrolling | only what the folder just opened brings in |

## Known upstream issues, not fixed yet

- `ListViewExt.qml`: the footer's height depends on its own position ("Binding loop detected for property implicitHeight"). Harmless.
- `BrowseDeviceView.qml`: "Unable to assign [undefined] to QQmlComponent*" for sections without a header. Harmless.
- Changing the audio filters and the VLC visualization at the same moment can stall the audio output (playing, but the time stands still). Terrikon switches them one after the other.