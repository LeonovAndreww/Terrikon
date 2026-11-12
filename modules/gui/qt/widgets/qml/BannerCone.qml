/*****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
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


import VLC.MainInterface
import VLC.Widgets as Widgets
import VLC.Style
import VLC.Util

Image {
    id: root

    required property color color

    // Terrikon: the heap's skip goes up the incline and tips its load at the
    // top; on hovering, on a click (which leads home), when asked
    property bool animated: false
    signal clicked()

    sourceSize: Qt.size(VLCStyle.icon_normal, VLCStyle.icon_normal)

    source: SVGColorImage.colorize("qrc:///misc/cone.svg").accent(root.color).uri()

    focus: false

    function play() {
        if (!skipRun.running)
            skipRun.start()
    }

    // from the drawing's coordinates (viewBox 18 16 220 220, moved up 12)
    function _x(x) { return (x - 18) / 220 * width }
    function _y(y) { return (y - 12 - 16) / 220 * height }

    // the skip: a small box tilted along the incline
    Item {
        id: skip
        property real t: 0      // 0 at the foot, 1 at the top
        property real tip: 0    // 0 upright, 1 tipped over the top
        visible: skipRun.running
        width: root.width * 0.15
        height: width * 0.6
        // on the incline (its edge from 32,170 to 118,84), outside the heap:
        // moved off the edge, up and left, by half its height
        readonly property real _off: 220 * (height / 2 + 1) / root.height * 0.7071
        x: root._x(34 + (114 - 34) * t - _off) - width / 2
        y: root._y(168 + (88 - 168) * t - _off) - height / 2
        rotation: -45 + tip * 80

        Rectangle {
            anchors.fill: parent
            radius: height * 0.2
            color: root.color
        }
    }

    // what it tips: a few lumps rolling down the far side
    Repeater {
        model: 3
        delegate: Rectangle {
            required property int index
            property real fall: 0
            visible: skipRun.running && fall > 0 && fall < 1
            width: root.width * 0.06
            height: width
            radius: width / 2
            color: root.color
            x: root._x(140 + fall * (60 + index * 14)) - width / 2
            y: root._y(86 + fall * (84 - index * 6)) - height / 2
            opacity: 1 - fall * 0.6

            SequentialAnimation on fall {
                running: skipRun.running
                PauseAnimation { duration: 1050 + index * 90 }
                NumberAnimation { from: 0; to: 1; duration: 520; easing.type: Easing.InQuad }
                PropertyAction { value: 0 }
            }
        }
    }

    SequentialAnimation {
        id: skipRun
        NumberAnimation { target: skip; property: "t"; from: 0; to: 1; duration: 900; easing.type: Easing.InOutSine }
        NumberAnimation { target: skip; property: "tip"; from: 0; to: 1; duration: 220; easing.type: Easing.OutQuad }
        PauseAnimation { duration: 420 }
        NumberAnimation { target: skip; property: "tip"; to: 0; duration: 200 }
        NumberAnimation { target: skip; property: "t"; to: 0; duration: 700; easing.type: Easing.InOutSine }
    }

    HoverHandler {
        enabled: root.animated
        cursorShape: Qt.PointingHandCursor
        onHoveredChanged: if (hovered) root.play()
    }

    TapHandler {
        enabled: root.animated
        onTapped: {
            root.play()
            root.clicked()
        }
    }
}
