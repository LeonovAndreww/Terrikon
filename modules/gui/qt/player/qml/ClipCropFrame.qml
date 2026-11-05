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

// What a vertical clip keeps, over the video: a 9:16 window to drag where
// the action is and to zoom with the wheel; the rest is dimmed.
Item {
    id: root

    visible: ClipSelection.active && ClipSelection.vertical && _video.width > 0

    // The video as shown: fitted into this item, letterboxed
    property size _source: Qt.size(0, 0)
    readonly property rect _video: {
        if (_source.width <= 0 || _source.height <= 0 || width <= 0 || height <= 0)
            return Qt.rect(0, 0, 0, 0)
        const scale = Math.min(width / _source.width, height / _source.height)
        const w = _source.width * scale
        const h = _source.height * scale
        return Qt.rect((width - w) / 2, (height - h) / 2, w, h)
    }

    function _readSource() {
        _source = TerrikonActions.videoSize()
    }

    onVisibleChanged: _readSource()
    Connections {
        target: ClipSelection
        function onVerticalChanged() { root._readSource() }
        function onActiveChanged() { root._readSource() }
    }

    // The crop window in this item's coordinates
    readonly property real _frameH: _video.height / ClipSelection.cropZoom
    readonly property real _frameW: _frameH * 9 / 16
    readonly property real _frameX: _video.x + (_video.width - _frameW) * ClipSelection.cropX
    readonly property real _frameY: _video.y + (_video.height - _frameH) * ClipSelection.cropY

    readonly property color _shade: Qt.rgba(0, 0, 0, 0.6)

    // Dimmed outside of the window
    Rectangle { color: root._shade; x: root._video.x; y: root._video.y
                width: root._frameX - root._video.x; height: root._video.height }
    Rectangle { color: root._shade; x: root._frameX + root._frameW; y: root._video.y
                width: root._video.x + root._video.width - x; height: root._video.height }
    Rectangle { color: root._shade; x: root._frameX; y: root._video.y
                width: root._frameW; height: root._frameY - root._video.y }
    Rectangle { color: root._shade; x: root._frameX; y: root._frameY + root._frameH
                width: root._frameW; height: root._video.y + root._video.height - y }

    Rectangle {
        id: frame

        x: root._frameX
        y: root._frameY
        width: root._frameW
        height: root._frameH

        color: "transparent"
        border.color: "white"
        border.width: VLCStyle.dp(2, VLCStyle.scale)


        MouseArea {
            anchors.fill: parent
            cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
            preventStealing: true

            property point _grab
            property real _grabX
            property real _grabY

            onPressed: (mouse) => {
                _grab = mapToItem(root, mouse.x, mouse.y)
                _grabX = ClipSelection.cropX
                _grabY = ClipSelection.cropY
            }

            onPositionChanged: (mouse) => {
                if (!pressed)
                    return
                const p = mapToItem(root, mouse.x, mouse.y)
                const roomX = root._video.width - root._frameW
                const roomY = root._video.height - root._frameH
                if (roomX > 0)
                    ClipSelection.cropX = _grabX + (p.x - _grab.x) / roomX
                if (roomY > 0)
                    ClipSelection.cropY = _grabY + (p.y - _grab.y) / roomY
            }

            onWheel: (wheel) => {
                const steps = wheel.angleDelta.y / 120
                ClipSelection.cropZoom = ClipSelection.cropZoom * Math.pow(1.1, steps)
            }
        }
    }
}
