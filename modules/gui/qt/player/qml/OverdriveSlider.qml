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

// A volume slider that stops at 100%. Pulling past the end meets some
// resistance: the track gives a little and springs back. Pulling on clicks
// it over to a 0-`limit` scale with a mark at 100%: the volume stays at
// 100% until the button is released, so it never leaps; the next drag
// goes higher. Released at or below 100%, it folds back to 0-100%.
Widgets.SliderExt {
    id: root

    // Width of the 0-100% part
    property real baseWidth: VLCStyle.dp(100, VLCStyle.scale)
    property real limit: 2
    property bool extended: false

    // How far past the end the pointer has to go to unlock
    readonly property real _unlockAt: VLCStyle.dp(36, VLCStyle.scale)
    // Furthest the track gives while locked
    readonly property real _give: VLCStyle.dp(14, VLCStyle.scale)

    // Pointer distance past the end while locked, and what the track shows
    // of it: the further you pull, the less it gives
    readonly property real _rawPull: (pointer.active && !extended)
                                     ? Math.max(0, pointer.point.position.x - width) : 0
    property real _pull: 0
    readonly property real _stretch: (1 - 1 / (_pull * 0.55 / _give + 1)) * _give

    // From the click over until release, the volume stays at 100%: a
    // slider keeps its position, not its value, when its range changes
    // under a drag, and sets the value from the pointer on release
    property bool _holding: false

    on_RawPullChanged: {
        _pull = _rawPull
        if (_rawPull > _unlockAt) {
            _holding = true
            extended = true
            value = 1
        }
    }

    onValueChanged: {
        if (_holding && value !== 1)
            value = 1
    }

    Behavior on _pull {
        enabled: !pointer.active
        SpringAnimation { spring: 5; damping: 0.35 }
    }

    implicitWidth: baseWidth

    from: 0
    to: extended ? limit : 1

    onPressedChanged: {
        if (pressed)
            return
        if (_holding) {
            _holding = false
            value = 1
        } else if (value <= 1) {
            // Same caveat: the range change must not move the value
            const kept = value
            extended = false
            value = kept
        }
    }

    // Watches the pointer without taking it from the slider
    PointHandler {
        id: pointer
        acceptedButtons: Qt.LeftButton
    }

    Translate {
        id: handleShift
        x: root._stretch
    }

    Component.onCompleted: {
        if (handle)
            handle.transform = [handleShift]
    }

    // The part of the track that gives while pulled
    Rectangle {
        parent: root.background
        visible: root._stretch > 0.5
        x: parent ? parent.width : 0
        width: root._stretch
        height: parent ? parent.height : 0
        radius: height / 2
        color: root.color
        opacity: 0.6
    }

    // Where 100% is, once unlocked
    Rectangle {
        parent: root.background
        visible: root.to > 1
        x: parent ? parent.width / root.to : 0
        width: VLCStyle.dp(2, VLCStyle.scale)
        height: parent ? parent.height * 3 : 0
        anchors.verticalCenter: parent ? parent.verticalCenter : undefined
        radius: width / 2
        color: root.color
        opacity: 0.8
    }
}
