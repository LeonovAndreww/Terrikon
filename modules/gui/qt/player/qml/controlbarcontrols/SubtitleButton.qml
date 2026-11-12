/*****************************************************************************
 * Copyright (C) 2026 Terrikon contributors
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

import VLC.Widgets as Widgets
import VLC.Style
import VLC.Player
import VLC.Util

// Subtitles on / off, as on video sites: shown only when the media has some,
// on by default (VLC picks a track by itself); the track the button turns
// back on is the one last shown
Widgets.IconToolButton {
    id: root

    visible: Player.subtitleTracks.count > 0
    width: visible ? implicitWidth : 0

    readonly property bool _on: Player.subtitleTracks.anySelected

    checked: _on
    text: VLCIcons.audiosub
    description: _on ? qsTr("Subtitles off") : qsTr("Subtitles on")

    onClicked: Player.subtitleTracks.toggleShown()

    // the "CC" plate of video players, filled while subtitles show
    contentItem: Item {
        implicitWidth: VLCStyle.icon_toolbar
        implicitHeight: VLCStyle.icon_toolbar

        Rectangle {
            anchors.centerIn: parent
            width: Math.round(parent.width * 0.86)
            height: Math.round(parent.height * 0.62)
            radius: VLCStyle.dp(3, VLCStyle.scale)
            color: root._on ? root.color : "transparent"
            border.color: root.color
            border.width: VLCStyle.dp(1.5, VLCStyle.scale)

            Text {
                anchors.centerIn: parent
                text: "CC"
                font.pixelSize: Math.round(parent.height * 0.62)
                font.bold: true
                color: root._on ? root.backgroundColor : root.color
            }
        }
    }

    AccessibleCompat.id: "subtitleButton"
}
