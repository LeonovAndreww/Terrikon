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
import VLC.Util
import VLC.Terrikon

// The clip range on the seek bar: a tinted span between two brackets that
// can be dragged. Lies over the bar, which is "barHeight" tall at y = 0.
Item {
    id: root

    property real barHeight: VLCStyle.dp(5, VLCStyle.scale)
    property color color: "#9F7BFF"

    // True while an edge is being dragged; the seek bar then ignores the pointer
    readonly property bool dragging: _pressedEdges > 0
    property int _pressedEdges: 0

    readonly property real _length: ClipSelection.mediaLength
    readonly property real _startX: _length > 0 ? ClipSelection.start / _length * width : 0
    readonly property real _endX: _length > 0 ? ClipSelection.end / _length * width : 0

    visible: ClipSelection.active && _length > 0

    function _timeAt(x) {
        return Helpers.clamp(x / width, 0, 1) * _length
    }

    // Selected span, slightly taller than the bar so it reads over the
    // played part
    Rectangle {
        x: root._startX
        width: Math.max(1, root._endX - root._startX)
        y: -(height - root.barHeight) / 2
        height: root.barHeight * 2
        radius: height / 2
        color: Qt.rgba(root.color.r, root.color.g, root.color.b, 0.45)
        border.color: root.color
        border.width: 1
    }

    // Edge 0 is the start, edge 1 the end
    Repeater {
        id: edges

        model: 2

        MouseArea {
            id: edge

            required property int index

            readonly property real time: index === 0 ? ClipSelection.start : ClipSelection.end
            property real _grabOffset: 0

            x: (index === 0 ? root._startX : root._endX) - width / 2
            y: -(height - root.barHeight) / 2
            width: VLCStyle.dp(16, VLCStyle.scale)
            height: root.barHeight * 5

            cursorShape: Qt.SizeHorCursor
            hoverEnabled: true
            preventStealing: true

            onPressed: (mouse) => { _grabOffset = mouse.x - width / 2 }
            onPressedChanged: root._pressedEdges += pressed ? 1 : -1
            onPositionChanged: (mouse) => {
                if (!pressed)
                    return
                // Where the edge centre should be: the pointer, less where it grabbed the edge
                // Edges stick to marked moments within a few pixels
                const snapDistance = VLCStyle.dp(8, VLCStyle.scale) / root.width * root._length
                const seconds = MomentMarks.snap(root._timeAt(x + mouse.x - _grabOffset), snapDistance)
                if (index === 0)
                    ClipSelection.start = seconds
                else
                    ClipSelection.end = seconds
            }
            // Show the frame at the edge once it is let go
            onReleased: Player.position = Helpers.clamp(time / root._length, 0, 1)

            Rectangle {
                anchors.centerIn: parent
                width: VLCStyle.dp(4, VLCStyle.scale)
                height: parent.height
                radius: width / 2
                color: root.color
                border.color: Qt.rgba(0, 0, 0, 0.5)
                border.width: 1
            }

            Rectangle {
                visible: edge.pressed || edge.containsMouse

                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.top
                anchors.bottomMargin: VLCStyle.margin_xxsmall

                width: label.implicitWidth + VLCStyle.margin_xsmall * 2
                height: label.implicitHeight + VLCStyle.margin_xxxsmall * 2
                radius: VLCStyle.dp(4, VLCStyle.scale)
                color: Qt.rgba(0, 0, 0, 0.75)

                Text {
                    id: label
                    anchors.centerIn: parent
                    text: ClipSelection.formatTime(edge.time)
                    color: "white"
                    font.pixelSize: VLCStyle.fontSize_small
                }
            }
        }
    }
}
