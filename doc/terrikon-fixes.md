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
| Services | Sorted by their modules' names ("fmc", "jamendo", "podcast"...), not by the names shown; two items placed first each came before the other, not a strict order | by the shown name, in the language's alphabet; a strict order |
| Audio output | WASAPI, a device whose channels are in another order (5.1, 7.1): a block that did not fit at once was kept, and the channels of its rest were put in order a second time; sound from the wrong speakers, clicks | `wasapi.c`: the rest of a block is played as it was left |
| Audio output | Changing the audio filters when the new chain could not be made: the clock delay was set on no chain, a crash | `dec.c`: set only on a chain |
| Audio output | A silence shorter than one sample made an empty block, which the outputs do not take (an assertion) | `dec.c`: no block then |
| Player | Setting the renderer that is already set released it before holding it: freed while in use when the player was its only owner | `player.c`: held first |
| Player | A track's delay waited in the input's queue without holding the track; the track could be gone when the input came to it | `player.c`, `input.c`: held until done or dropped |
| Core, Windows | When no interface could be started, the process waited forever with no window | `winvlc.c`: it says so and leaves |
| Core, Windows | The language of the registry was read without its type or the end of its string | `winvlc.c`: both checked, the key closed |
| Core, Windows | One instance: the message with the files to open was read with no regard to its size (any window may send one), and its memory was neither checked nor freed by the sender | `win_msg.c`, `specific.c`: every length is checked against what came |
| Network | The old HTTP access: an answer with `cnonce` when none was sent, or without `rspauth`, was compared with nothing: a crash a server could cause | `http_auth.c`: refused |
| Media library | `vlc_ml_count_show_episodes` counted the albums of a genre; `vlc_ml_count_media_labels` asked for the list and gave its address as the count, the list lost | `vlc_media_library.h`: the right queries |
| Interface | Messages of Qt come from any thread; while closing, the handler read what was being cleared | `vlcqtmessagehandler.cpp`: the handlers running are counted and waited for |
| Interface | At start, the test of the graphics API put off for later used variables of a block that had ended by then | `qt.cpp`: they are copied |
| Interface | A cover fetched: the cover's state was changed from the fetcher's thread; and closing with a fetch still waiting locked the player twice, the program never left | `player_controller.cpp`: done on the interface thread, nothing locked in the callback |
| Interface | The timed lyrics of a media without any read an empty value (undefined behaviour) | `player_controller.cpp`: an empty list |
| Interface | Dialogs: a question asked just as the interface closed could miss the wake up and wait forever | `dialogmodel.cpp`: the state is changed under the lock it is waited with |
| Interface | With the mouse wheel over the speed button before its slider exists: "event is not defined" | `PlaybackSpeedButton.qml` |
| Interface | The buttons of the bar that depend on programs or on a menu being shown did not follow them | `control_list_filter.cpp`: listens to both |
| Interface | The entries under a nested entry of the navigation were those of its parent | `navigationmodel.cpp` |
| Interface | The light theme's secondary background was the color "#lightGrey400", which is none | `systempalette.cpp` |
| Interface | A texture's vertical wrap mode was set as its horizontal one | `qsgtextureview.cpp` |
| Interface | The luminance of an image counted blue as 0.144, not 0.114 | `imageluminanceextractor.cpp` |
| Interface | The Windows guard of the platform compositor was `__WIN32` | `_WIN32` |
| Interface | After a drop that was refused, the busy cursor stayed | `ListViewExt.qml` |
| Playlist | While searching, a tap or the keyboard played another item: the row shown was taken for the row of the playlist | `PlaylistDelegate.qml`, `PlaylistPane.qml`: mapped to the source |
| Playlist | The total length stayed as it was when the lengths became known; a change of the current item's title or cover was not told | `playlist_model.cpp`, `playlist_controller.cpp` |
| Playlist | The copy kept to recover the playlist after a crash: its name went through Latin-1 (wrong in a folder with other letters, as a Cyrillic user name), and the last good copy was deleted before the new one was in place | `model_recovery_agent.hpp`: QString and QFile all the way, the old copy kept until the new one is there |
| Network views | The folders of the user were sorted with an order that is not strict (undefined behaviour of `std::sort`) | `standardpathmodel.cpp` |
| Network views | The children of a node were read after the tree was unlocked; an item removed was looked up by name, so that another of the same name could go; a device was kept when its last source left, and removed while it still had some | `networkmediamodel.cpp`, `networkdevicemodel.cpp` |
| Media library | A media gone since its list was made came as an empty media to the playlist (a crash); a callback asking for the same items again was lost; a group whose list failed was read anyway | `medialib.cpp`, `mlvideogroupsmodel.cpp` |
| Media library | Bookmarks: the row was remembered across the answer of the library (another bookmark changed, or a read out of the list); no player crashed; a field of the interface thread was read from the player's | `mlbookmarkmodel.cpp` |
| Media library | An artist's page could show the answer for the artist left before; the busy mark of a playlist page was never shown; dropping a link onto the playlists used a name that does not exist | `MusicArtist.qml`, `PlaylistMediaDisplay.qml`, `PlaylistMediaList.qml` |
| Media library | Home: moving through the favourites left the scrolling animation on for good | `HomePage.qml` |
| Toolbar editor | Removing profiles removed one less than asked (none for one) though the views were told | `controlbar_profile_model.cpp` |
| Preferences | A language in the registry that is not in the list: the list was read at -1 | `simple_preferences.cpp` |
| Video output | HDR10: the metadata were given to DXGI with the size of a pointer, not of the metadata; the screen was told nothing, or part of it | `dxgi_swapchain.cpp`: the size of the structure |
| Video output | Direct3D11: a first pixel shader that failed was hidden by the second one succeeding; the staging textures were lost when their views could not be made | `d3d11_shaders.cpp`, `direct3d11.cpp` |
| Player | Deleting the main track while another of its kind stays selected (two audio tracks at once): the "selected" event named the deleted track | `es_out.c`: the one that stays |
| Audio filters | The 16 bit to 64 bit float converter wrote 32 bit floats: noise | `format.c` |
| Audio filters | Equalizer presets with negative levels that are not whole (Dance, Classical, Headphones...): the part after the point was printed as a huge unsigned number | `equalizer.c`: the sign apart |
| Audio filters | SoXR worked in place when the frames out are larger than the frames in: written past the block | `soxr.c` |
| Audio filters | Scaletempo kept its queue through a seek and through normal speed: a bit of the old sound later; its pitch variant left its callback behind when the resampler failed | `scaletempo.c`: a flush, the queue dropped at speed 1 |
| Audio filters | Mono with more than two channels, the spatializer with one: written past the block; the volume normalizer with a history of 0: a move of a huge size | `mono.c`, `spatializer.cpp`, `normvol.c`: refused or bounded |
| Demuxers, packetizers | Read or written out of bounds, or a crash, on what a file says: FLAC (a block size cut short, extradata under 4 bytes), PNG (a text key under 17 bytes), H.264 (sizes that wrap to a division by zero), H.266 (a fragment freed then queued), AV1 (the OBUs of the configuration written one over the other), Matroska (a chapter script whose uid is no number: an exception nobody caught), SCTE-27 in TS (a short section), AVI (the palette loop counted the wrong variable), a plane copy that never ended when a line is larger than the cache | `flac.h`, `flac.c`, `png.c`, `h264_nal.c`, `h266.c`, `av1_obu.c`, `chapter_command_script.cpp`, `ts_scte.c`, `bitmapinfoheader.h`, `copy.c` |
| Core | Exporting a playlist to a file that cannot be made: `fclose(NULL)`; a cookie with an empty `Path=` read the byte before the path; moving playlist items with a target that wraps passed the bound | `export.c`, `httpcookies.c`, `request.c` |
| LibVLC | `libvlc_media_player_get_abloop` read times that were never set when there is no loop; the interpolated time was given even when the interpolation failed | `media_player.c` |
| Interface | The taskbar buttons: COM was left once more than entered (a temporary holder copied) | `mainctx_win32.cpp`: made in place |
| Interface | The renderers' model read at row -1; a media library list that failed was gone through anyway (covers of genres and folders) | `renderer_manager.cpp`, `mlhelper.hpp` |
| Interface | Going back with no history yet took an entry out of an empty list | `navigation_history.cpp` |
| Interface | The subtitles' frame rate box listened to the delay of the second subtitles and divided where it should multiply; the audio tracks of a video's information showed its video tracks; the teletext buttons called a method colors do not have; the switch named a `control` that does not exist for the screen readers; an extension's menu was read one past the list; the blur divided by zero on an image one pixel wide | `TracksPageSubtitle.qml`, `VideoInfoExpandPanel.qml`, `TeletextWidget.qml`, `CheckButton.qml`, `extensions_manager.cpp`, `fast_gaussian_blur_template.h` |
| Media library | `getDataById` for an item that is not there (deleted meanwhile) left its promise waiting for ever: the rejection was never reached | `mlbasemodel.cpp`: the answer is passed on also when empty |
| Media library | The window stopped answering for 20 to 50 s whenever a grid showed a video kept on a network drive or host that does not answer (measured: 48.9 s on Home, then again after each playback). `ExpandGridView` builds every item with `getDataAt()`, which works out all the roles, and the `isDeletableFile` role asked the file system for the rights of the file and of its folder, on the interface thread | `mlmediamodel.cpp`: asked through `terrikon::writableSoon()`, on a thread of its own, waited for 150 ms at most; a drive that keeps one question waiting is not asked more. Measured after: 0.7 s at worst (the start itself) |
| Player controls | `TypeError: Cannot read property 'item' of null` at each playback start: `ControlRepeater` connected its `forceUnlock` to a function of the control, a connection with no receiver that outlived the control and called it after its end | `ControlRepeater.qml`: a `Connections` of the delegate calls the control |
| Core, media sources | The window stopped answering for as long as a services discovery took to open (21 s measured, opening Files while a network drive did not answer): `vlc_media_source_Hold()` and `Release()` took the provider's lock, which `vlc_media_source_provider_GetMediaSource()` keeps while it opens a module, and the interface thread holds and releases sources | `media_source.c`: the count is atomic; only finding a source and dropping its last reference take the lock. Measured after: 34 ms |
| Volumes (Windows) | The drives were asked for their labels while the module was opened, network drives included: one whose host does not answer kept the opening (and the lock above) for 20 s and more. A drive that could not be asked (no disc in the reader, a network drive not connected) ended the listing: the module failed to open and no drive was shown at all | `winvolumes.c`: the drives are listed by the module's thread; a network drive is not asked, it is named from its mapping ("share (\\host) (Z:)", as the file explorer does); a drive without an answer is left out, the others stay |
| Input (Windows) | Stopping a file of a network drive or host that does not answer took about 40 s, and the next file asked for meanwhile started only then: while an input is opened, each access and demux module tried asks the disk (libdvdcss, libbluray and the C runtime among them), each question waits as long as Windows tries, and the input's interruption reaches none of them | `input.c`: from the stop until the opening ends, the input thread is made to give up its synchronous requests (`CancelSynchronousIo`, every 50 ms). `filesystem.c`: `vlc_open()`, `vlc_stat()` and `vlc_opendir()` are given up on the interruption of their thread's context (new `vlc_interrupt_watch_start/stop()`), or 2 s after it was killed. Measured after: stopped in 0.27 s, the next file playing 15 ms after it was asked |
| Audio effects: stereo widener | Moving the delay while playing read the audio output as a filter (a wrong sample rate for the new buffer) and reallocated the buffer under the audio thread's feet (use after free); the crossfeed slider changed the feedback instead | `stereo_widen.c`: a lock between the audio thread and the callbacks, the rate kept at opening, crossfeed set (AU-17, AU-18) |
| Audio effects: delay (chorus/flanger) | Moving the delay or the sweep depth while playing reallocated the delay line under the audio thread's feet, and the write pointer stayed in the old buffer (or past the end of a shorter one) | `chorus_flanger.c`: a lock between the audio thread and the callbacks, the write pointer put back at the start of the new buffer (AU-21, AU-22) |
| Media library: files opened directly stay in the history after they are deleted | A file played before its folder was in the library is kept as an external media, which no rescan looks at: once deleted, it stayed in "Continue watching" for ever, without a thumbnail, and failed to open | `medialibrary.cpp`: after each discovery, a thread of its own forgets the external media whose local file is gone while its folder is still there (a folder gone may be a drive unplugged; shares and drive letters given to them are not asked) |
| Single instance (Windows) | Only a player started from a file took the next files: a player started on its own, then a file opened from the explorer, gave two players, the second without its media library (its base was the first's: a bare Home, a small odd window). A start with nothing to open always made a second player | `specific.c`, `win_msg.c`: every instance takes the next ones' files; a start without any brings the running player to the front (`AllowSetForegroundWindow`); the mutex and window are named per installation, so a copy elsewhere stays a player of its own |
| Window buttons: minimize and maximize did not light up on hovering | Their hover colour was the background's, half seen: on a dark bar, nothing showed (close has red of its own) | `CSDWindowButtonSet.qml`: the text colour, faint, as Windows does |
| Icon buttons (the video in the library's corner, ...) kept the arrow cursor | `IconButton` had no cursor of its own, unlike the other buttons | `IconButton.qml`: the hand on hovering |
| Volume past 100% lost at each start (Windows) | The audio session keeps up to 100%; mmdevice applied the rest as a gain it did not keep | `mmdevice.c`: the gain kept in the settings (`mmdevice-boost`) and given back at opening |
| Playback stopped for good after seeking: pause and seeks went unanswered (seen after a long pause, a jump to the end, then seeks) | The hardware decoder waited for a free surface with no time limit; with all of them held by pictures waiting for a clock that waited for that decoder (the input buffering after the seek), nothing moved again. VLC 3 gave up after a second | `va_surface.c`: the wait ends after 1 s, the frame is lost instead |
| After a long pause, playback jumped to the end of the file (a USB drive) | Windows powers a USB drive down after 20 minutes idle; the first reads after it fail, and any read error was taken for the end of the file | `file.c`: on a read error the file is opened again where it was, for some 10 s, until the input stops (not reproduced at will: the drive must sleep) |
| Library folders: a folder added right after another stayed out of the list | The list was asked once at a time: a change arriving while one was on its way was dropped | `mlfoldersmodel.cpp`: listed once more after it |
| Library context menu: "Information" and "Media Information" both shown for a video | Two entries for the same window | `MLContextMenu.qml`: the second only where the first is missing |
| The first second of a media shown in milliseconds ("671 ms / 00:13") | A length not known yet (0) counted as a media shorter than a second | `VLCTick::isSubSecond`: above 0 only |
| Home rows: a click on a card scrolled the page under the pointer | The row scrolled itself into view on any focus | not on a focus given by the mouse |
| A song without an album took the cover of another album lying in the same folder (Folder.jpg in Downloads) | The folder art finder takes the folder's picture for any file | `meta_engine/folder.c`: for files with an album only |
| MP4 | stts/ctts: a second allocation that fails freed the first array twice (here and with the box) | freed with the box only |
| MP4 | tfra: a box cut short kept the number of entries announced; a seek walked the zeroed ones | the entries really read |
| MP4 | tfra v1: a seek skipped every other entry (stepped by two over 64-bit arrays); it gave the offset of a fragment with the time of the next | one by one; the time that goes with the offset |
| MP4 | sidx: a seek counted the subsegments from 0, not from the earliest presentation time | from that time |
| MP4 | A seek in a chunk after the first landed on the chunk's first sample: the sample's absolute index was compared with the chunk's length | compared with the chunk's end |
| MP4 | A chapter number out of the list was read past the end of the array | refused |
| MP4 | Size of a stream unknown (network): read uninitialized; cmov: an uncompressed box that is no moov was added as NULL, the root's last child left pointing at the freed moov | size 0 = unknown; refused; the last child found again |
| Matroska | GotoAndPlay: a command without its ) threw out of the parser (std::out_of_range); a well formed one with a short uid was ignored (offset compared with a length); the depth guard was not given back on early returns | bounds checked first, the whole uid read, the depth given back on every way out |
| Windows | Second instance: payload allocation failure written through NULL | nothing sent |
| Player | Track lists: a row -1 read before the start of the list | refused |
| Qt | Leaving the services page while the add-ons were still looked for froze the window: the add-ons manager was let go of on the interface thread, which waited for the finder held by a name lookup or a slow server | let go of on a thread of its own; its late answers dropped |
| Qt | A page without a list (none of its own initialIndex) given the place in a list kept for it: "Cannot assign to non-existent property" | given only to a page having it |
| Linux X11, the Qt X11 compositor: the video stayed black under the interface (Linux Mint 22 Cinnamon, Mesa radeonsi). With Qt's default GLX integration the interface surface came out opaque; upstream at the same commit behaves the same | QT_XCB_GL_INTEGRATION=xcb_egl unless the user set it: EGL keeps the alpha |
| Input | A file read over a slow network (an SMB share giving 1 MB/s to a 6 MB/s video): the demuxer blocked in its read while the outputs ran dry, and the clock went on: the time ran over a frozen picture, the sound was dropped as late, then the late PCR reset the clock and the time jumped back | a stall watch: the outputs paused while the demuxer waits with the decoders empty, on again once 1 s is gathered (2, 4, 8 s for the next stalls); the PCRs met meanwhile and the bursts after it are not taken for discontinuities |
| Qt, Windows | A maximized window that stayed a popup the size of the screen (back from full screen) lost 8 px on each side for a sizing frame it does not have: the desktop showed unblurred all around, and the acrylic layer was shifted by as much | the frame taken off only for a window with a sizing frame (WM_NCCALCSIZE and the acrylic surface) |
| Player bar | A video without a cover shows the note of music | the video placeholder |
| Input clock | A seek while paused, then play: the time stood still for a while, the sound out of sync (the new clock reference, dated during the pause, shifted on resuming by the whole pause: a clock gap) | the reference of a paused input dated at the pause, as while rebuffering |

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
| An update of FFmpeg or yt-dlp could leave none: the working program was deleted before the new one was in place, and of FFmpeg's two programs one could be new and one old | `components.cpp`: the old ones are moved aside and come back if anything fails |
| "Downloading" forever when tar could not be started; yt-dlp's own update, when it could not be started, was not tried again for a day | no `finished` comes after a failed start; both now handle it |
| The Components dialog (and the preferences opening it) stopped the interface while each program was asked its version, up to 4 s each | asked without waiting |
| "Off" in the list of subtitles could switch another track off | the place among the streams (for ffmpeg) was used as the row of the list; the states now carry the row too |
| The wave preview of the tracks popover did not follow the track chosen | it called a method, which tells no change; it now depends on the revision |
| Vertical clip of a video narrower than 9:16: the frame shown was wider than the video | the frame was made from the height alone; now as ffmpeg crops |
| Changing a clip's settings while its size was measured could stop the interface for a second | the meter was waited for after being killed |
| Several links and files added at once did not keep their order | files were added at once and each page when yt-dlp answered; they now go in in the order asked |
| A track's own volume: the track waited in the input's queue without being held, and the volume was remembered even when the input did not take it | `player.c`, `input.c` |
| Qt messages while closing could reach the journal being deleted | `diagnostics.cpp`: the handlers running are counted and waited for |
| A large cover was decoded whole on the interface thread for the visualizer's colour | read at 24 pixels from the start |
| A folder chip of Home with "#" or "?" in its name opened another path | its URL is encoded part by part |
| Coming back from the player to an opened folder of Videos: "Cannot assign to non-existent property initialIndex", and the place in the folder was lost | the page remembered its place in the history as the other video pages do, but had no property to take it back |
| Holding the button for 2x: the speed stayed at 2x when the release never came (the window left with the button down, the video ended meanwhile) | the hold only ended on the release; it now also ends when the playback stops or the application is left |
| The clip popover could stop the interface for up to 5 s the first time: ffmpeg was asked for its encoders on the interface thread; the list was also read and filled from two threads | asked aside when the clip panel is made, once (`std::call_once`) |
| A thread left looking at a media file (`offThread`) could come back to code that was gone: VLC unloads its plugins on the way out, a drive that does not answer keeps such a thread for tens of seconds | the plugin is kept in memory once such a thread is started (`keepPluginLoaded`) |
| Hovering a library tile whose file is gone showed "Your media can't be opened" on the screen, once per hover | the preview's player reported its errors like the main one; it is now silent (`no_interact`): a tile without a preview tells enough |
| The logo (it leads home) kept the arrow cursor | `BannerCone.qml`: the hand while it leads home |
| Picture in picture: a tiny speaker and the volume shown as 0.3794642984867096 | a volume of its own; it is now the player's control (`VolumeWidget`: icon, percent, tracks and boost on hovering) |
| The volumes of the tracks played together were not kept from one video to the next (the tracks chosen were) | kept by the track's name, as the tracks on are (`TrackListModel`, `-volumes`) |
| A clip sounded quieter than the video it was cut from, with the boost past 100% | the boost was not in the clip; it is now, in the clip panel too ("Boost"), applied as the player does (the volume cubed) |
| Hovering a tile whose file is gone showed nothing at all | the tile tells why: the file deleted or moved, its folder gone, the drive not connected or another plugged in, the share not answering, no access |
| The media library lost its folders, one replacing the other when added (builds of 2026-11-14 night) | the single player logic of that night also ran in the preparser processes: each after the first took itself for a second start and went away, folders could not be read and the library dropped them. Only the player takes part now (`--terrikon-player`, given by its executable) |
| A drive that does not answer while reading: playback just stopped, with nothing said | a "The drive does not answer" window after a second of retries, with Cancel; it closes by itself when the reading goes on |
| Previews: dragging along the time bar or the volume grabbed the tile instead | the tile's drag took the pointer over; both bars keep it now (`preventStealing`) |
| Previews: the volume percent stayed in the middle of the bar, the speaker only told muted or not | the percent follows the level, the speaker shows the level as the player's does |
| Videos: the cards of a folder with other letters than Latin ones ("Своё") were never joined | the folder's key was compared encoded on one side, decoded on the other |
| Music mini window: the wave of the card stopped short of its sides and bottom | to the edges, kept inside the rounded corners by the visualizer itself (a first try with `QtQuick.Effects` left the window empty: that module is not in the build) |
| Videos: the joined cards of a folder showed a notch where its last row was shorter than the one above | one background per folder drawn under its cards (a few rectangles for the rows in view) instead of one per card, each asking its four neighbours |
| Home: "Videos" and "Music" were offered though in the library already | an answer to the first request (made before the library's folders were known, slow to count the files) came after the right one and was shown over it; older answers are dropped, folders are compared as real paths and those inside a library folder are not offered |
| Player: no hand pointer on the previous/next bands, the elapsed/remaining time, the mini window's time bar | the pointer of the setting on each |
| Visualizations panel stayed open over another media, even a video | closed when the media changes |
| "Fainter layers behind" showed for every style, did something for the hills only | shown for the hills, named for what it does |
| Lists: a double click (or Enter) did not open the player; Back then led to a half empty page | "player" was pushed as a page of the history, which the page loader no longer knows |
| A song played from the library (an artist with one song) got its whole folder queued around it (105 files of Downloads) | the folder queue is for files opened from outside only (`LibraryAppend`) |
| Lyrics: "Find" answered "no lyrics in the file" though it had found them; a second click showed them | the state said found before the lyrics were in |
| An empty tool tip box over the lyrics note; tool tips at the top of the window covered their button | no tool tip without a text; under the item when there is no room above |
| Music mini window: no buttons in the round shape; shown over the player playing that song; not shown on Alt+Tab, never again once closed | buttons and time on the cover, wheel for the sound; shown when the player is away (minimized, another window in front), the cross holds until the player is back |
| "Open containing folder" did not select the file | explorer /select, from the library and the play queue |
| The window's own menu (right click on the title) was light in the dark theme | uxtheme's preferred app mode follows the player's theme |
| Folder queue in the explorer's order: names given in another form than a wide string were dropped, and files the window did not list (filtered, cut short) left out of the queue | any form converted (StrRetToStrW); those files queued after, by name |
| Home offers: each refresh started its own thread on the drives (a dead network drive: one more held thread each time), and the plugin could be unloaded under them | one at a time, the last request run when it ends; the plugin held |
| Lyrics: a late answer for a song played, then another, then that one again was taken as current; a file chosen while another song started was given to it; the file was read on the window's thread | answers tied to the playing they were asked for; the file read away from the window |
| Back from a video stalled it: its surface gone, the output was set up again and taken for a new video, the player opened again | the video is paused on any way out of the player; its output coming back is not a new video |
| A right click on the logo opened the window's own menu too | the logo's clicks are its own |
| Linux: forgetting the media whose file is gone did not build (no sys/stat.h), and would have skipped every file, any path starting with / taken for a share | the share check and the separator are Windows only; / elsewhere |
| Linux: the video window of its own (no embedding, software OpenGL) was still titled "VLC media player" | titled Terrikon, on X11 and Wayland |
| Linux: with the automation on, quitting hung for good: closing a socket does not wake a thread blocked in accept() or recv() on Linux, and the interface waited for that thread | shutdown() on the sockets first |
| Settings, Components: yt-dlp's version stayed blank on a busy machine (yt-dlp, a Python program, took 8 s; it was killed at 4 s), and the process was destroyed while still running (a Qt warning) | 30 s, nothing waits for it; the process deleted only once it has ended |
| Automation on Linux: no interface window found (`at`, `items`...), as the X11 compositor renders it offscreen, neither top-level nor visible | also take a window rendered for another one (QQuickRenderControl::renderWindowFor) |
| Player: the transparency sliders (and speed, sound waves) could only be found in the menu of the subtitles button | a player settings button (gear) with them and Effects and filters; it takes the effects button's place in the bars, saved bars included || Seek bar waves and thumbnails: ffmpeg read a whole big file on a network drive (up to three times: waves, key frames, every frame) beside the playback; on a slow share the video stalled and the next file took half a minute to open | for a file over 48 MB on a network drive, the quick thumbnails only (16 seeks); the waves stay, ffmpeg seeks over the video (1 MB read out of 140) |
| Seek bar thumbnails: the quick pass (16 frames sought one by one) never ran, the seek bar never gave the strip the length: every file was read through for its key frames first | the length given, waited for while the file opens |

| Music started from a row of Home (New music) played alone: the next one never came | the track and those after it in the row go to the queue |
| Music: no full screen, the button stayed disabled without a video output | the window itself takes the screen |
| Music: the volume said twice, VLC's label at the top right and the toast | the toast only, as for a video |
| Music in its own window: the dark cover on hover did not beat with the picture; resizing decoded the picture again at each step (black, late, small) | it beats with it; the picture decoded once at a fixed size |
| Visualization: after the power was unplugged and plugged back, it was drawn once a second (in its window and in the full screen player); the frames of the window stopped coming (cause not reproduced) | drawn on a timer of its own when the frames stop, noted in the journal |
| Most of Terrikon's Russian strings (426) were not translated in the builds: make merged ru.po with the stale vlc.pot, newer by a few ms after a git checkout, and dropped them | the build keeps ru.po the newer |
| Back from a video: it paused under the bar instead of going on in the small view | it goes on there, by default |
| Sound and tracks popovers hard to read: the opacity of the overlays on the picture | the panels' opacity |
| Clip export: a black square around the progress | the bar's own background removed |
## Known upstream issues, not fixed yet

- `ListViewExt.qml`: the footer's height depends on its own position ("Binding loop detected for property implicitHeight"). Harmless.
- `BrowseDeviceView.qml`: "Unable to assign [undefined] to QQmlComponent*" for sections without a header. Harmless.
- Changing the audio filters and the VLC visualization at the same moment can stall the audio output (playing, but the time stands still). Terrikon switches them one after the other.