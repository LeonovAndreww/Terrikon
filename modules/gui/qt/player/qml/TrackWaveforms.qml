/*****************************************************************************
 * Copyright (C) 2026 Terrikon authors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
import QtQuick

import VLC.Player
import VLC.Style
import VLC.Terrikon

// The waveform of each audio track that is playing, along the seek bar,
// each in its own shade. The part already played is drawn bright, the
// rest dimmed: the same picture twice, the bright one cut at the playback
// position, so playing never repaints it.
Item {
    id: root

    required property WaveformLoader loader

    // Played part, 0..1
    property real position: 0

    property bool shown: false

    // Track colours by track index, repeated when there are more tracks
    property var colors: [ "#ffffff", "#d0d0d0" ]

    // How the waves look, see WaveformView.Style
    property int style: WaveformView.Curve

    // Indexes of the tracks being played, re-read when tracks are switched
    property int _tracksRevision: 0
    readonly property var _playing: {
        void _tracksRevision
        void Player.audioTracks.count
        return Player.audioTracks.trackStates().filter(t => t.enabled).map(t => t.index)
    }

    Connections {
        target: Player.audioTracks
        function onDataChanged() { root._tracksRevision++ }
    }

    visible: opacity > 0
    opacity: (shown && loader && loader.trackCount > 0 && _playing.length > 0) ? 1 : 0

    Behavior on opacity {
        NumberAnimation { duration: VLCStyle.duration_short }
    }

    WaveformView {
        anchors.fill: parent
        opacity: 0.4
        loader: root.loader
        colors: root.colors
        tracks: root._playing
        style: root.style
    }

    Item {
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        width: parent.width * Math.max(0, Math.min(1, root.position))

        clip: true

        WaveformView {
            width: root.width
            height: root.height
            loader: root.loader
            colors: root.colors
            tracks: root._playing
            style: root.style
        }
    }
}
