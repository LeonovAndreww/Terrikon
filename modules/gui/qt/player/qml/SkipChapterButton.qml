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
    opacity: _available ? 1 : 0
    enabled: _available

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
    }
}
