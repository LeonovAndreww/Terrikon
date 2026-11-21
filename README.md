# Terrikon

**Terrikon** is a personal fork of VLC 4 (the `master` branch) with a new Qt/QML
interface, built for Windows first; Linux builds and plays too. The name is
the cone-shaped spoil heap of a coal mine, like the VLC cone. Free, never sold.

## What differs from VLC 4

- **Several audio tracks at once**, each with its own volume (and above 100%
  on Windows). The fork began here, for game replays with separate tracks.
- **Clips**: loop, size, save and copy, tracks mixed, vertical 9:16 with a crop.
- **Seek bar**: previews, per-track waveforms, moment marks, chapters, jump hints ("+10 s").
- **Picture in picture**: in the window or in a window of its own; a mini window for music.
- **Music player**: round cover, a big cover with "Details" and "Next in queue"
  cards, synced lyrics (LRCLIB or `.lrc`), own visualizer (ring, bars, wave,
  hills, mirror, line) that can also sit over a video.
- **Home**: folder chips, "Continue watching", a search that takes links.
- **Videos**: folder tree, grouped cards, hover previews.
- **Window transparency**: acrylic, frosted, clear glass.
- **Full Russian translation**, dark dialogs, Fluent menu icons.
- **Network drives that do not answer** no longer freeze the window or playback.
- **One player per install**; a queue made from the folder in the Explorer's order.

## Bugs of VLC 4 fixed

The full table (more than 200 entries, with the cause and the file of each) is in
[`doc/terrikon-fixes.md`](doc/terrikon-fixes.md). In short, group 1 (bugs VLC 4
shows as it is) and group 2 (bugs of VLC 4 that Terrikon brought out):

- **Player and clock**: after a seek the time stood still for a second; a seek
  while paused put the audio out of sync; "play and pause" stuck at the end;
  playback stopped for good after a seek (hardware decoder waited with no
  limit); a USB drive that powered down made the file "end" early; a slow
  network share made the time run over a frozen picture.
- **Hangs on dead network drives**: 20 to 50 s freezes of the window when a
  grid showed a file on a host that does not answer, and a 40 s stop of such a file.
- **Crashes and memory safety**: use after free and out-of-bounds writes in audio
  filters (stereo widener, delay, SoXR, mono, spatializer, normvol, scaletempo,
  equalizer), demuxers and packetizers (FLAC, PNG, H.264, MP4, Matroska), the old
  HTTP access, cookies, playlist export; random crashes from Qt's PCRE2 JIT
  stack under MinGW.
- **Single instance (Windows)**: a second VLC hung at start (same pipe names in
  every process); files opened from the Explorer made two players.
- **Direct3D11 / video output**: a leak of about 12 MB per close (and 5 to 15 MB
  per picture-in-picture round); HDR10 metadata passed with the wrong size.
- **Interface**: lists ran from Z to A by default; sort arrow upside down;
  names sorted by code points ("Episode 10" before "Episode 2"); hotkeys dead
  on a non-Latin layout; about 1450 strings missing from the Russian
  translation; service names untranslated; drives without a label unnamed;
  "-1 ms" for media of unknown length; radio stations without flags and logos.
- **Media library**: nested model resets in bookmarks, media gone since the list
  was made crashing the playlist, files opened directly staying in the history
  after deletion, a folder added right after another missing from the list.
- **Audio output**: WASAPI channel order for 5.1/7.1 applied twice; empty
  blocks from a sub-sample silence; a crash when a filter chain could not be made.
- **Linux X11**: the video stayed black under the interface with Qt's default GLX integration.

---

# VLC media player

**VLC** is a libre and open source **media player** and **multimedia engine**,
focused on **playing everything**, and **running everywhere**.

**VLC** can play most multimedia files, discs, streams, devices and is also able to
convert, encode, **stream** and manipulate streams into numerous formats.

VLC is used by many over the world, on numerous platforms, for very different use cases.

The **engine of VLC** can be embedded into 3rd party applications, and is called *libVLC*.

**VLC** is part of the [VideoLAN project](https://videolan.org) and
is developed and supported by a community of volunteers.

The VideoLAN project was started at the university [École Centrale Paris](https://www.centralesupelec.fr/) who
relicensed VLC under the GPLv2 license in February 2001. Since then, VLC has
been downloaded **billions** of times.

## License

**VLC** is released under the GPLv2 *(or later)* license.
*On some platforms, it is de facto GPLv3, because of the licenses of dependencies*.

**libVLC**, the engine is released under the LGPLv2 *(or later)* license. \
This allows embedding the engine in 3rd party applications, while letting them to be licensed under other licenses.

# Platforms

VLC is available for the following platforms:
- [Windows] *(from 7 and later, including UWP platforms and all versions of Windows 10)*
- [macOS] *(10.10 and later)*
- [GNU/Linux] and affiliated
- [BSD] and affiliated
- [Android] *(4.2 and later)*, including Android TV and Android Auto
- [iOS] *(9 and later)*, including AppleTV and iPadOS
- Haiku, OS/2 and a few others.

[Windows]: https://www.videolan.org/vlc/download-windows.html
[macOS]: https://www.videolan.org/vlc/download-macosx.html
[GNU/Linux]: https://www.videolan.org/vlc/#download
[BSD]: https://www.videolan.org/vlc/download-freebsd.html
[Android]: https://www.videolan.org/vlc/download-android.html
[iOS]: https://www.videolan.org/vlc/download-ios.html

Not all platforms receive the same amount of care, due to our limited resources.

**Nota Bene**: The [Android app](https://code.videolan.org/videolan/vlc-android/) and
the [iOS app](https://code.videolan.org/videolan/vlc-ios/) are located in different repositories
than the main one.

# Contributing & Community

**VLC** is maintained by a community of people, and VideoLAN is not paying any of them.\
The community is composed of developers, helpers, maintainers, designers and writers that want
this open source project to thrive.

The main development of VLC is done in the C language, but this repository also contains
plenty of C++, Obj-C, asm and Rust.

Other repositories linked to vlc are done in languages including Kotlin/Java [(Android)](https://code.videolan.org/videolan/vlc-android/),
Swift [(iOS)](https://code.videolan.org/videolan/vlc-ios/), and C# [(libVLCSharp)](https://code.videolan.org/videolan/libvlcsharp/).

We need help with the following tasks:
- Coding
- Packaging for Windows, macOS and Linux distributions
- Technical writing for the documentation
- Design
- Support
- Community management and communication.

Please contribute :)

We are on IRC. You can find us on the **#videolan** channel on *[Libera.chat]*.

[Libera.chat]: https://libera.chat

## Contributions

Contributions are now done through Merge Requests on our [GitLab repository](https://code.videolan.org/videolan/vlc/).

CI and discussions should be resolved before a Merge Request can be merged.

# libVLC

**libVLC** is an embeddable engine for 3rd party applications and frameworks.

It runs on the same platforms as VLC *(and sometimes on more)* and can provide playback,
streaming and conversion of multimedia files and streams.


**libVLC** has numerous bindings for other languages, such as C++, Python and C#.

# Support

## Links

Some useful links that might help you:

- [VLC web site](https://www.videolan.org/vlc/)
- [Support](https://www.videolan.org/support/)
- [Forums](https://forum.videolan.org/)
- [Wiki](https://wiki.videolan.org/)
- [Developer's Corner](https://wiki.videolan.org/Developers_Corner)
- [VLC hacking guide](https://wiki.videolan.org/Hacker_Guide)
- [Bugtracker](https://code.videolan.org/videolan/vlc/-/issues)
- [VideoLAN web site](https://www.videolan.org/)

## Source Code sitemap
```
ABOUT-NLS          - Notes on the Free Translation Project.
AUTHORS            - VLC authors.
COPYING            - The GPL license.
COPYING.LIB        - The LGPL license.
INSTALL            - Installation and building instructions.
NEWS               - Important modifications between the releases.
README             - Project summary.
THANKS             - VLC contributors.

bin/               - VLC binaries.
bindings/          - libVLC bindings to other languages.
compat/            - compatibility library for operating systems missing
                     essential functionalities.
contrib/           - Facilities for retrieving external libraries and building
                     them for systems that don't have the right versions.
doc/               - Miscellaneous documentation.
extras/analyser    - Code analyser and editor specific files.
extras/buildsystem - Different build system specific files.
extras/misc        - Files that don't fit in the other extras/ categories.
extras/package     - VLC packaging specific files such as spec files.
extras/tools/      - Facilities for retrieving external building tools needed
                     for systems that don't have the right versions.
include/           - Header files.
lib/               - libVLC source code.
modules/           - VLC plugins and modules. Most of the code is here.
po/                - VLC translations.
share/             - Common resource files.
src/               - libvlccore source code.
test/              - Testing system.
```
