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
import VLC.Widgets as Widgets
import VLC.Terrikon

// Frame under the pointer while hovering the seek bar, cut out of the
// thumbnail sheets of a ThumbnailStrip.
Item {
    id: root

    required property ThumbnailStrip strip

    // Time to show, in seconds
    property real seconds: 0

    property bool shown: false

    // The frame shown; kept while fading out, so no empty box flashes
    property var _frame: ({})
    readonly property bool _hasFrame: _frame.source !== undefined
    readonly property var _wanted: (strip && strip.ready && shown) ? strip.frameAt(seconds) : null
    on_WantedChanged: {
        if (_wanted && _wanted.source !== undefined)
            _frame = _wanted
    }

    Connections {
        target: root.strip
        function onReadyChanged() {
            if (!root.strip.ready)
                root._frame = ({})
        }
    }

    implicitWidth: VLCStyle.dp(160, VLCStyle.scale)
    implicitHeight: VLCStyle.dp(90, VLCStyle.scale)

    visible: opacity > 0
    opacity: (shown && _hasFrame) ? 1 : 0

    Behavior on opacity {
        NumberAnimation { duration: VLCStyle.duration_veryShort }
    }

    // The cell of the sheet, cut out on the GPU, rounded and outlined in
    // one pass so the picture follows the rounded edge
    Widgets.EnhancedImageExt {
        anchors.fill: parent

        source: root._hasFrame ? root._frame.source : ""
        textureSubRect: root._hasFrame ? Qt.rect(root._frame.x, root._frame.y,
                                                 root._frame.width, root._frame.height)
                                       : undefined

        radius: VLCStyle.dp(6, VLCStyle.scale)
        backgroundColor: "black"
        // Thin light edge so a dark frame still reads against dark video
        borderColor: Qt.rgba(1, 1, 1, 0.85)
        borderWidth: VLCStyle.dp(2, VLCStyle.scale)

        // Sheets are small JPEGs reused for every hover position
        asynchronous: false
        cache: true
        smooth: true
    }
}
