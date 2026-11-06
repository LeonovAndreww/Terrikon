/*****************************************************************************
 * Copyright (C) 2026 the Terrikon authors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

import QtQuick
import QtQuick.Window

import VLC.MainInterface
import VLC.Widgets as Widgets
import VLC.Style
import VLC.Util
import VLC.Player
import VLC.Playlist
import VLC.Menus
import VLC.Terrikon

// Terrikon: a small round window over all the others while music plays:
// the cover, and the ring of the visualizer beating around it. It is moved
// by dragging, opens the player on a double click, pauses on a click, and
// goes away from its menu on a right click.
Window {
    id: root

    readonly property int side: VLCStyle.dp(230, VLCStyle.scale)

    width: side
    height: side
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool
    title: qsTr("Visualizer")

    visible: Player.playingState !== Player.PLAYING_STATE_STOPPED

    Component.onCompleted: {
        const p = Visualizer.miniPosition
        const screen = root.screen
        if (p.x >= 0 && p.y >= 0) {
            x = p.x
            y = p.y
        } else if (screen) {
            x = screen.virtualX + screen.width - width - VLCStyle.dp(40, VLCStyle.scale)
            y = screen.virtualY + screen.height - height - VLCStyle.dp(80, VLCStyle.scale)
        }
    }

    AudioVisualizer {
        id: ring

        anchors.fill: parent
        style: 1 // the ring
        cover: cover
        color: "#ff7a1a"
        color2: "#ffe2bf"
    }

    // A dark disc behind the cover, so it reads on any desktop
    Rectangle {
        anchors.centerIn: cover
        width: cover.width + VLCStyle.dp(6, VLCStyle.scale)
        height: width
        radius: width / 2
        color: Qt.rgba(0, 0, 0, 0.55)
        scale: cover.scale
    }

    Widgets.ImageExt {
        id: cover

        anchors.centerIn: parent
        width: Math.round(root.side * 0.48)
        height: width
        radius: width / 2
        scale: 1 + ring.level * 0.05

        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        cache: false
        sourceSize: Qt.size(width * 2, height * 2)
        source: (Player.artwork && Player.artwork.toString())
                ? VLCAccessImage.uri(Player.artwork) : VLCStyle.noArtAlbumCover
    }

    MouseArea {
        anchors.centerIn: parent
        width: cover.width + VLCStyle.dp(60, VLCStyle.scale)
        height: width

        acceptedButtons: Qt.LeftButton | Qt.RightButton
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor

        property point _press
        property bool _moved: false

        onPressed: (mouse) => {
            _press = Qt.point(mouse.x, mouse.y)
            _moved = false
        }
        onPositionChanged: (mouse) => {
            if (!pressed || _moved || !(mouse.buttons & Qt.LeftButton))
                return
            if (Math.abs(mouse.x - _press.x) + Math.abs(mouse.y - _press.y) > 4) {
                _moved = true
                root.startSystemMove()
            }
        }
        onReleased: {
            if (_moved)
                Visualizer.miniPosition = Qt.point(root.x, root.y)
        }
        onClicked: (mouse) => {
            if (mouse.button === Qt.RightButton)
                menu.popup(Qt.point(root.x + mouse.x, root.y + mouse.y),
                           [Player.playingState === Player.PLAYING_STATE_PLAYING ? qsTr("Pause") : qsTr("Play"),
                            qsTr("Open the player"), qsTr("Close the mini window")])
            else if (!_moved)
                clickTimer.restart()
        }
        onDoubleClicked: (mouse) => {
            clickTimer.stop()
            root._openPlayer()
        }
        onWheel: (wheel) => {
            if (wheel.angleDelta.y > 0)
                Player.setVolumeUp()
            else if (wheel.angleDelta.y < 0)
                Player.setVolumeDown()
        }

        Widgets.ToolTipExt {
            colorContext.palette: VLCStyle.palette
            visible: parent.containsMouse && !parent.pressed && text !== ""
            text: Player.title
        }
    }

    // A click pauses, unless a second one makes it a double click
    Timer {
        id: clickTimer
        interval: Qt.styleHints.mouseDoubleClickInterval
        onTriggered: MainPlaylistController.togglePlayPause()
    }

    StringListMenu {
        id: menu
        ctx: MainCtx
        onSelected: (index, text) => {
            if (index === 0)
                MainPlaylistController.togglePlayPause()
            else if (index === 1)
                root._openPlayer()
            else
                Visualizer.miniWindow = false
        }
    }

    function _openPlayer() {
        MainCtx.intfMainWindow.show()
        MainCtx.intfMainWindow.raise()
        MainCtx.intfMainWindow.requestActivate()
        MainCtx.playerView = true
    }
}
