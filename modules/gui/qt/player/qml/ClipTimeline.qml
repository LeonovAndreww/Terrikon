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

// A close-up time scale for the clip: it shows a window around the range,
// so the edges can be placed finely. Dragging an edge to a side of the
// scale widens the window; once the range is small in it, the window
// closes in again around it.
Item {
    id: root

    property color color: "white"
    property color secondaryColor: Qt.rgba(1, 1, 1, 0.6)

    // The window shown, in seconds
    property real viewStart: 0
    property real viewEnd: 1
    readonly property real _viewLength: Math.max(0.001, viewEnd - viewStart)
    readonly property real _media: ClipSelection.mediaLength

    // The range takes about half of the window when it is fitted
    function _fit() {
        const length = Math.max(ClipSelection.duration * 2, 4)
        let s = (ClipSelection.start + ClipSelection.end - length) / 2
        s = Math.max(0, Math.min(s, _media - length))
        viewStart = s
        viewEnd = Math.min(_media, s + length)
    }

    function _x(seconds) {
        return (seconds - viewStart) / _viewLength * width
    }

    function _seconds(x) {
        return viewStart + Math.max(0, Math.min(width, x)) / width * _viewLength
    }

    // Edge being dragged: 0 the start, 1 the end, -1 none
    property int _dragging: -1
    // The pointer is at a side of the scale while dragging
    property bool _atEdge: false

    Behavior on viewStart { enabled: root._dragging < 0; NumberAnimation { duration: VLCStyle.duration_long; easing.type: Easing.OutCubic } }
    Behavior on viewEnd { enabled: root._dragging < 0; NumberAnimation { duration: VLCStyle.duration_long; easing.type: Easing.OutCubic } }

    implicitHeight: VLCStyle.dp(44, VLCStyle.scale)

    Connections {
        target: ClipSelection
        function onActiveChanged() {
            if (ClipSelection.active)
                root._fit()
        }
        function onRangeChanged() {
            if (root._dragging >= 0)
                return
            // Small in the window, or out of it: close in again
            const share = ClipSelection.duration / root._viewLength
            if (share < 0.2 || ClipSelection.start < root.viewStart || ClipSelection.end > root.viewEnd)
                root._fit()
        }
    }

    Component.onCompleted: _fit()

    // Pulled to a side while dragging: the window widens on that side
    Timer {
        id: widen
        interval: 60
        repeat: true
        running: root._dragging >= 0 && root._atEdge
        onTriggered: {
            const step = root._viewLength * 0.06
            if (root._dragging === 0) {
                root.viewStart = Math.max(0, root.viewStart - step)
                ClipSelection.start = root.viewStart
            } else {
                root.viewEnd = Math.min(root._media, root.viewEnd + step)
                ClipSelection.end = root.viewEnd
            }
        }
    }

    // Track
    Rectangle {
        id: track
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        height: VLCStyle.dp(22, VLCStyle.scale)
        radius: VLCStyle.dp(4, VLCStyle.scale)
        color: Qt.rgba(1, 1, 1, 0.08)

        // Click: play from there
        MouseArea {
            anchors.fill: parent
            onClicked: (mouse) => {
                if (root._media > 0)
                    Player.position = root._seconds(mouse.x) / root._media
            }
        }
    }

    // The range
    Rectangle {
        x: root._x(ClipSelection.start)
        width: Math.max(1, root._x(ClipSelection.end) - x)
        anchors.top: track.top
        anchors.bottom: track.bottom
        color: Qt.rgba(1, 1, 1, 0.16)
        border.color: root.color
        border.width: VLCStyle.dp(2, VLCStyle.scale)
        radius: track.radius
    }

    // Playback position
    Rectangle {
        visible: x >= 0 && x <= root.width
        x: root._x(Player.position * root._media)
        width: VLCStyle.dp(2, VLCStyle.scale)
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        color: root.color
        opacity: 0.8
    }

    // The two edges of the range
    Repeater {
        model: 2

        Rectangle {
            id: handle

            required property int index
            readonly property bool isStart: index === 0

            x: root._x(isStart ? ClipSelection.start : ClipSelection.end) - width / 2
            width: VLCStyle.dp(10, VLCStyle.scale)
            anchors.top: track.top
            anchors.bottom: track.bottom
            radius: VLCStyle.dp(3, VLCStyle.scale)
            color: root.color

            MouseArea {
                anchors.fill: parent
                anchors.margins: -VLCStyle.dp(6, VLCStyle.scale)
                cursorShape: Qt.SizeHorCursor
                preventStealing: true

                onPressed: root._dragging = handle.index
                onPositionChanged: (mouse) => {
                    if (!pressed)
                        return
                    const x = mapToItem(root, mouse.x, 0).x
                    const edge = VLCStyle.dp(6, VLCStyle.scale)
                    root._atEdge = handle.isStart ? x <= edge : x >= root.width - edge
                    const t = root._seconds(x)
                    if (handle.isStart)
                        ClipSelection.start = Math.min(t, ClipSelection.end - 0.1)
                    else
                        ClipSelection.end = Math.max(t, ClipSelection.start + 0.1)
                }
                onReleased: {
                    root._dragging = -1
                    root._atEdge = false
                    // Settle the window around the result
                    if (ClipSelection.duration / root._viewLength < 0.2)
                        root._fit()
                    if (!handle.isStart && root._media > 0)
                        Player.position = Math.max(ClipSelection.start, ClipSelection.end - 3) / root._media
                }
            }
        }
    }

    // Times at the sides of the window
    Text {
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.bottomMargin: -VLCStyle.dp(4, VLCStyle.scale)
        text: ClipSelection.formatTime(root.viewStart)
        color: root.secondaryColor
        font.pixelSize: VLCStyle.fontSize_xsmall
    }
    Text {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.bottomMargin: -VLCStyle.dp(4, VLCStyle.scale)
        text: ClipSelection.formatTime(root.viewEnd)
        color: root.secondaryColor
        font.pixelSize: VLCStyle.fontSize_xsmall
    }
}
