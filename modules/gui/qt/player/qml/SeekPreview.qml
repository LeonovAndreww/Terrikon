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

// Frame under the pointer while hovering the seek bar, cut out of the
// thumbnail sheets of a ThumbnailStrip.
Item {
    id: root

    required property ThumbnailStrip strip

    // Time to show, in seconds
    property real seconds: 0

    property bool shown: false

    readonly property var _frame: (strip && strip.ready && shown) ? strip.frameAt(seconds) : ({})
    readonly property bool _hasFrame: _frame.source !== undefined

    implicitWidth: VLCStyle.dp(160, VLCStyle.scale)
    implicitHeight: VLCStyle.dp(90, VLCStyle.scale)

    visible: opacity > 0
    opacity: (shown && _hasFrame) ? 1 : 0

    Behavior on opacity {
        NumberAnimation { duration: VLCStyle.duration_veryShort }
    }

    Rectangle {
        id: frameBox

        anchors.fill: parent

        radius: VLCStyle.dp(6, VLCStyle.scale)
        color: "black"
        // Thin light edge so a dark frame still reads against dark video
        border.color: Qt.rgba(1, 1, 1, 0.85)
        border.width: VLCStyle.dp(2, VLCStyle.scale)

        Item {
            id: viewport

            anchors.fill: parent
            anchors.margins: frameBox.border.width

            clip: true

            readonly property real _scale: root._hasFrame ? width / root._frame.width : 1

            // The whole sheet, shifted so the wanted cell fills the viewport
            Image {
                source: root._hasFrame ? root._frame.source : ""

                x: root._hasFrame ? -root._frame.x * viewport._scale : 0
                y: root._hasFrame ? -root._frame.y * viewport._scale : 0
                width: implicitWidth * viewport._scale
                height: implicitHeight * viewport._scale

                // Sheets are small JPEGs reused for every hover position
                asynchronous: false
                cache: true
                smooth: true
                mipmap: false
            }
        }
    }
}
