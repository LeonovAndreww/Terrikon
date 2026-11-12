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

import VLC.Widgets as Widgets
import VLC.Style
import VLC.Player
import VLC.Util
import VLC.Terrikon

// Opens or closes clip mode
Widgets.ImageToolButton {
    id: clipBtn

    sourceSize: Qt.size(VLCStyle.icon_toolbar, VLCStyle.icon_toolbar)

    enabled: Player.isStarted && Player.seekable
    checked: ClipSelection.active

    // Scissors, white; underlined like other active toggles when clip mode is on
    imageSource: SVGColorImage.colorize("qrc:///icons/clip.svg")
                     .color1(clipBtn.checked ? clipBtn.colorContext.fg.primary
                                             : Qt.alpha(clipBtn.colorContext.fg.primary, 0.75)).uri()

    description: qsTr("Clip") + TerrikonActions.hint("clip-toggle")

    onClicked: {
        if (ClipSelection.active) {
            ClipSelection.close()
        } else {
            const length = Player.length.toSeconds()
            ClipSelection.begin(Player.position * length, length)
        }
    }

    Rectangle {
        visible: clipBtn.checked
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        width: parent.width * 0.6
        height: VLCStyle.dp(2, VLCStyle.scale)
        radius: height / 2
        color: clipBtn.colorContext.fg.primary
    }

    AccessibleCompat.id: "clipButton"
}
