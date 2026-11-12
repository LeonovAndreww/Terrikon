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
// off by default (unless switched on last time); the track the button turns
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

    // Terrikon: on and off told apart whatever the accent (white by default,
    // the colour of the icons): dim when off, a dot under it when on
    opacity: checked ? 1.0 : 0.55
    Rectangle {
        visible: parent.checked
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: -VLCStyle.dp(1, VLCStyle.scale)
        width: VLCStyle.dp(4, VLCStyle.scale)
        height: width
        radius: width / 2
        color: parent.color
    }


    AccessibleCompat.id: "subtitleButton"
}
