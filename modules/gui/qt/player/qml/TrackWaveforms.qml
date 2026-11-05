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

import VLC.Style
import VLC.Terrikon

// One waveform per audio track along the seek bar, each in its own colour.
// The part already played is drawn bright, the rest dimmed: the same
// picture twice, the bright one cut at the playback position, so playing
// never repaints it.
Item {
    id: root

    required property WaveformLoader loader

    // Played part, 0..1
    property real position: 0

    property bool shown: false

    // Track colours, repeated when there are more tracks
    property var colors: [ "#4FC3F7", "#F48FB1", "#AED581", "#B39DDB" ]

    visible: opacity > 0
    opacity: (shown && loader && loader.trackCount > 0) ? 1 : 0

    Behavior on opacity {
        NumberAnimation { duration: VLCStyle.duration_short }
    }

    // Keeps the bars readable over bright video
    Rectangle {
        anchors.fill: parent
        radius: VLCStyle.dp(4, VLCStyle.scale)
        color: Qt.rgba(0, 0, 0, 0.35)
    }

    WaveformView {
        anchors.fill: parent
        opacity: 0.35
        loader: root.loader
        colors: root.colors
        barStep: VLCStyle.dp(3, VLCStyle.scale)
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
            barStep: VLCStyle.dp(3, VLCStyle.scale)
        }
    }
}
