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
| Player | After a seek the time and the seek bar freeze for about a second, then jump | `src/player/timer.c`: the first point after a seek is sent at once |
| Player | Some media show a duration of -1 ms | duration only shown when known |
| Media library | Bookmarks model nests a reset inside a reset each time a media starts ("beginResetModel called ... without calling endResetModel first") | `mlbookmarkmodel.cpp`: no second refresh inside the reset |
| Interface | Service names (network, devices) not translated | `qtr` instead of `qfu` for module long names |
| Interface | Random visualization switching | visualization chosen from a menu |
| Interface | Hotkeys do nothing with a non-Latin keyboard layout | keys matched by their native scan code |
| Interface | About 1450 strings missing from the Russian translation | translated |
| Core | `spawn.c` lost the child's error output | kept |
| Core | Signals emitted while a lock is held | emitted after unlocking |
| Browse | A drive without a label (usually C:) has no name; labels in other languages come out garbled (ANSI call) | "Label (E:)", "Local disk (C:)", read with the wide call |
| Browse | On Windows the Documents folder is shown as "Home"; Desktop and Downloads share their translations with unrelated strings ("Screen", the verb "Download") | each folder keeps its own name |
| Tables | The sort arrow points up for Z to A, unlike the file explorer | up is A to Z; a new column starts from A to Z |

## Our own bugs (introduced by Terrikon, fixed)

| Bug | Cause |
|-----|-------|
| Videos and the player failed to load (b89e9d3) | missing QML imports, a grouped property set on an instance |
| Blurry simple preferences icons | our SVG icons were scaled with VLC's pixmap code meant for its PNGs |
| Pink (magenta) tool tips over the folder chips on Home and the folder buttons on covers | tool tips placed directly without the palette VLC's own ones carry |
| Leaving the player while a visualization played stopped the music; once a grey view was left behind (not seen again since) | our leaving code stopped playback; now music goes on |

## Known upstream issues, not fixed yet

- `ListViewExt.qml`: the footer's height depends on its own position ("Binding loop detected for property implicitHeight"). Harmless.
- `BrowseDeviceView.qml`: "Unable to assign [undefined] to QQmlComponent*" for sections without a header. Harmless.
- Changing the audio filters and the VLC visualization at the same moment can stall the audio output (playing, but the time stands still). Terrikon switches them one after the other.
