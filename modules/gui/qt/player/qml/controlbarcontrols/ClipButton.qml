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
Widgets.IconToolButton {
    id: clipBtn

    enabled: Player.isStarted && Player.seekable
    checked: ClipSelection.active
    // TODO: a scissors glyph; the A-B loop one is the closest in the icon font
    text: VLCIcons.atob
    description: qsTr("Clip") + " (" + TerrikonActions.keyFor("clip-toggle") + ")"

    onClicked: {
        if (ClipSelection.active) {
            ClipSelection.close()
        } else {
            const length = Player.length.toSeconds()
            ClipSelection.begin(Player.position * length, length)
        }
    }

    AccessibleCompat.id: "clipButton"
}
