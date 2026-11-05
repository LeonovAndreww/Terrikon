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
import QtQuick.Templates as T

import VLC.Player
import VLC.Style

// "Skip opening" over the video while an opening, ending, recap or preview
// chapter plays, as streaming sites do. Chapters are recognised by name,
// so this works for files whose release named them.
T.Button {
    id: root

    // Skippable chapter at the playback position, empty map if none
    property var _segment: ({})
    // Chapter the viewer chose to watch: the button stays away for it
    property int _dismissedIndex: -1

    readonly property bool _available: _segment.kind !== undefined && _segment.index !== _dismissedIndex

    // True while the player controls are on screen: after its countdown the
    // button only comes back with them, as on streaming sites
    property bool controlsShown: false

    // Seconds the button stays on its own once a chapter starts
    readonly property int timeout: 8
    // Countdown left, 1 to 0; drawn as a fill draining inside the button
    property real _left: 1
    property int _countdownFor: -1

    readonly property bool _shown: _available && (_left > 0 || controlsShown || hovered)

    on_SegmentChanged: {
        // Out of the chapter: coming back to it (seeking) counts down again
        if (_segment.index === undefined)
            _countdownFor = -1
        else if (_segment.index !== _countdownFor) {
            _countdownFor = _segment.index
            _left = 1
            _runCountdown()
        }
    }

    // Runs the rest of the countdown from where it stopped
    function _runCountdown() {
        countdown.stop()
        countdown.duration = timeout * 1000 * _left
        countdown.start()
    }

    NumberAnimation {
        id: countdown
        target: root
        property: "_left"
        to: 0
    }

    // Hovering holds the countdown; it goes on a moment after the pointer leaves
    onHoveredChanged: {
        if (hovered) {
            resumeTimer.stop()
            countdown.stop()
        } else if (_left > 0) {
            resumeTimer.restart()
        }
    }

    Timer {
        id: resumeTimer
        interval: 1000
        onTriggered: root._runCountdown()
    }

    function _update() {
        _segment = Player.hasChapters ? Player.chapters.skippableAt(Player.position) : ({})
    }

    Connections {
        target: Player
        function onPositionChanged() { root._update() }
        function onInputChanged() {
            root._dismissedIndex = -1
            root._update()
        }
    }

    text: {
        switch (_segment.kind) {
        case "opening": return qsTr("Skip opening")
        case "ending": return qsTr("Skip ending")
        case "recap": return qsTr("Skip recap")
        case "preview": return qsTr("Skip preview")
        }
        return ""
    }

    visible: opacity > 0
    opacity: _shown ? 1 : 0
    enabled: _shown

    Behavior on opacity {
        NumberAnimation { duration: VLCStyle.duration_short }
    }

    // Templates leave sizing to the style
    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    padding: VLCStyle.margin_small
    leftPadding: VLCStyle.margin_normal
    rightPadding: VLCStyle.margin_normal

    font.pixelSize: VLCStyle.fontSize_large
    font.weight: Font.DemiBold

    onClicked: {
        // Past the last chapter there is nothing to skip to but the end
        Player.position = Math.min(_segment.end, 0.9999)
    }

    // Right click: keep watching this one
    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: root._dismissedIndex = root._segment.index
    }

    contentItem: Text {
        text: root.text
        font: root.font
        color: "black"
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle {
        radius: VLCStyle.dp(6, VLCStyle.scale)
        color: root.hovered ? "white" : Qt.rgba(1, 1, 1, 0.88)
        border.color: root.visualFocus ? "black" : "transparent"
        border.width: VLCStyle.dp(2, VLCStyle.scale)

        // What is left of the countdown
        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: parent.width * root._left
            radius: parent.radius
            color: Qt.rgba(0, 0, 0, 0.12)
            visible: root._left > 0
        }
    }
}
