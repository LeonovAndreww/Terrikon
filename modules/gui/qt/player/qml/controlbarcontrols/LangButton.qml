/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
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

Widgets.IconToolButton {
    id: root

    // Proprerties

    readonly property var _parentItem: {
        if ((typeof rootPlayer !== 'undefined') && (rootPlayer !== null))
            return rootPlayer
        else
            return g_mainDisplay
    }

    // Signals

    signal requestLockUnlockAutoHide(bool lock)
    signal menuOpened(var menu)

    // Settings

    text: VLCIcons.audiosub

    enabled: menuLoader.status === Loader.Ready

    description: qsTr("Audio and subtitles")

    AccessibleCompat.id: "langButton"

    // Events

    // The press that closes an open menu (outside of it) comes right
    // before this click; it must not open it again
    property double _closedAt: 0

    onClicked: {
        if (menuLoader.item.opened)
            menuLoader.item.close()
        else if (Date.now() - _closedAt > 300)
            menuLoader.item.open()
    }

    // Children

    Loader {
        id: menuLoader

        sourceComponent: TracksPopover {
            id: menu

            parent: root

            x: 0
            y: -height - VLCStyle.margin_small

            focus: true

            onOpened: {
                root.requestLockUnlockAutoHide(true)
                root.menuOpened(menu)
            }

            onClosed: {
                root._closedAt = Date.now()
                root.requestLockUnlockAutoHide(false)
                root.forceActiveFocus()
                root.menuOpened(null)
            }
        }
    }

    function forceUnlock() {
        if((menuLoader)&&(menuLoader.item))
            menuLoader.item.close()
    }
}
