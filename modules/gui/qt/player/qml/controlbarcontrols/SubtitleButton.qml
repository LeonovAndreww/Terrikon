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

// Subtitles: a click switches them on or off (the track last shown comes
// back); resting the pointer on it, or a right click, opens the menu of the
// tracks (which subtitles, their delay, a file of them; the sound tracks
// too), the former "Audio and subtitles" button. Off by default, unless
// switched on last time.
Widgets.IconToolButton {
    id: root

    readonly property var _parentItem: {
        if ((typeof rootPlayer !== 'undefined') && (rootPlayer !== null))
            return rootPlayer
        else
            return g_mainDisplay
    }

    signal requestLockUnlockAutoHide(bool lock)
    signal menuOpened(var menu)

    readonly property bool _has: Player.subtitleTracks.count > 0
    readonly property bool _on: Player.subtitleTracks.anySelected

    checked: _on
    text: VLCIcons.audiosub
    description: !_has ? qsTr("Subtitles and tracks")
                       : (_on ? qsTr("Subtitles off") : qsTr("Subtitles on"))

    // off: dim and struck through
    opacity: (_has && _on) ? 1.0 : 0.55
    Rectangle {
        visible: !(root._has && root._on)
        anchors.centerIn: parent
        width: parent.width * 0.62
        height: VLCStyle.dp(1.5, VLCStyle.scale)
        radius: height / 2
        rotation: -45
        color: root.color
    }

    onClicked: {
        if (_has)
            Player.subtitleTracks.toggleShown()
        else
            _open()
    }

    // the menu: on resting the pointer, or a right click
    function _open() {
        if (menuLoader.item && !menuLoader.item.opened)
            menuLoader.item.open()
    }

    Timer {
        id: hoverOpen
        interval: 450
        onTriggered: root._open()
    }
    Timer {
        id: hoverClose
        interval: 500
        onTriggered: {
            if (menuLoader.item && menuLoader.item.opened && !menuLoader.item.pointerInside && !root.hovered)
                menuLoader.item.close()
        }
    }
    onHoveredChanged: {
        if (hovered) {
            hoverClose.stop()
            hoverOpen.restart()
        } else {
            hoverOpen.stop()
            hoverClose.restart()
        }
    }

    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: root._open()
    }

    Loader {
        id: menuLoader

        sourceComponent: TracksPopover {
            id: menu

            property bool pointerInside: menuHover.hovered

            parent: root

            x: 0
            y: -height - VLCStyle.margin_small

            focus: true

            HoverHandler {
                id: menuHover
                onHoveredChanged: if (!hovered) hoverClose.restart()
            }

            onOpened: {
                root.requestLockUnlockAutoHide(true)
                root.menuOpened(menu)
            }

            onClosed: {
                root.requestLockUnlockAutoHide(false)
                root.menuOpened(null)
            }
        }
    }

    function forceUnlock() {
        if (menuLoader.item)
            menuLoader.item.close()
    }

    AccessibleCompat.id: "subtitleButton"
}
