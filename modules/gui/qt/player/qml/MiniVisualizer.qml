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

// Terrikon: a small window over all the others while something plays, in
// one of four shapes:
//  - a card: the cover, and on hover the title, the buttons and the time
//  - round: the cover in a circle with the ring beating around it
//  - a strip: the cover, the title and the buttons over the visualization
//  - the visualization alone, a line or bars hanging over the desktop
// It is moved by dragging, resized from its corner (each shape keeps its own
// size), and its menu changes the shape or closes it. The visualization is
// the one chosen for music in the player.
Window {
    id: root

    readonly property int shape: Visualizer.miniShape
    // The visualization chosen in the player, the ring when there is none
    readonly property int chosenStyle: Visualizer.style !== 0 ? Visualizer.style : 1
    readonly property bool hovered: hover.hovered
    readonly property bool playing: Player.playingState === Player.PLAYING_STATE_PLAYING
    readonly property url coverSource: (Player.artwork && Player.artwork.toString())
                                       ? VLCAccessImage.uri(Player.artwork) : VLCStyle.noArtAlbumCover

    readonly property color accent: Visualizer.color
    readonly property color accent2: Visualizer.color2

    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool
    // A window of its own: not hidden or minimized along with the player
    transientParent: null
    title: qsTr("Visualizer")

    minimumWidth: shape === 2 ? 200 : 80
    minimumHeight: shape === 2 ? 44 : (shape === 3 ? 24 : 80)

    // Music only: a video goes on in picture in picture with its picture
    visible: Player.playingState !== Player.PLAYING_STATE_STOPPED && Player.videoTracks.count === 0

    function _applySize() {
        const s = Visualizer.miniSize(shape)
        width = s.width
        height = s.height
    }

    onShapeChanged: _applySize()

    Component.onCompleted: {
        _applySize()
        const p = Visualizer.miniPlace(Qt.size(width, height))
        x = p.x
        y = p.y
    }

    // The size is kept a moment after resizing stops
    onWidthChanged: sizeSaver.restart()
    onHeightChanged: sizeSaver.restart()
    Timer {
        id: sizeSaver
        interval: 500
        onTriggered: Visualizer.setMiniSize(root.shape, Qt.size(root.width, root.height))
    }

    // Off, and not shown again on minimizing until the player is back
    function _close() {
        Visualizer.miniWindow = false
        Visualizer.miniDismissed = true
    }

    function _openPlayer() {
        MainCtx.intfMainWindow.show()
        MainCtx.intfMainWindow.raise()
        MainCtx.intfMainWindow.requestActivate()
        MainCtx.playerView = true
    }

    function _showMenu(globalPos) {
        const mark = (on) => on ? "✓  " : "     "
        const items = [root.playing ? qsTr("Pause") : qsTr("Play"), qsTr("Open the player")]
        const shapes = Visualizer.miniShapeNames
        for (let i = 0; i < shapes.length; ++i)
            items.push(mark(i === root.shape) + qsTr("Shape: %1").arg(shapes[i]))
        items.push(qsTr("Close the mini window"))
        menu.popup(globalPos, items)
    }

    StringListMenu {
        id: menu
        ctx: MainCtx
        onSelected: (index, text) => {
            const shapes = Visualizer.miniShapeNames.length
            if (index === 0)
                MainPlaylistController.togglePlayPause()
            else if (index === 1)
                root._openPlayer()
            else if (index < 2 + shapes)
                Visualizer.miniShape = index - 2
            else
                root._close()
        }
    }

    // Everything sits in here, so the hover covers the whole window
    Item {
        id: content
        anchors.fill: parent

        HoverHandler { id: hover }

        // Windows lets the mouse through fully transparent pixels: an all
        // but invisible ground keeps the whole window in reach
        Rectangle {
            anchors.fill: parent
            radius: 10
            color: Qt.rgba(0, 0, 0, 1 / 255)
        }

        // Moving, the menu, a double click for the player; a click pauses
        // where there are no buttons
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            cursorShape: VLCStyle.pointerCursor

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
                    root._showMenu(Qt.point(root.x + mouse.x, root.y + mouse.y))
                else if (!_moved && (root.shape === 1 || root.shape === 3))
                    clickTimer.restart()
            }
            onDoubleClicked: {
                clickTimer.stop()
                root._openPlayer()
            }
            onWheel: (wheel) => {
                if (wheel.angleDelta.y > 0)
                    Player.setVolumeUp()
                else if (wheel.angleDelta.y < 0)
                    Player.setVolumeDown()
            }
        }

        Timer {
            id: clickTimer
            interval: Qt.styleHints.mouseDoubleClickInterval
            onTriggered: MainPlaylistController.togglePlayPause()
        }

        Loader {
            anchors.fill: parent
            sourceComponent: [cardShape, roundShape, stripShape, bareShape][root.shape] ?? cardShape
        }

        // Resizing from the bottom right corner
        MouseArea {
            width: 18
            height: 18
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            visible: root.hovered
            cursorShape: Qt.SizeFDiagCursor
            onPressed: root.startSystemResize(Qt.RightEdge | Qt.BottomEdge)

            Text {
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: 2
                text: "◢"
                font.pixelSize: 10
                color: Qt.rgba(1, 1, 1, 0.6)
            }
        }

        // Back to the player and close, as in the picture in picture of a
        // video
        Row {
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 6
            spacing: 4
            visible: root.hovered
            z: 1

            RoundButton {
                size: 22
                image: SVGColorImage.colorize("qrc:///icons/compact_player_exit.svg").color1("white").uri()
                tip: qsTr("Back to the player")
                onClicked: root._openPlayer()
            }
            RoundButton {
                size: 22
                icon: VLCIcons.close
                tip: qsTr("Close")
                onClicked: root._close()
            }
        }
    }

    // --- pieces shared by the shapes

    component RoundButton: Item {
        id: button

        property string icon
        property url image
        property string tip
        property real size: 32
        signal clicked()

        width: size
        height: size

        Rectangle {
            anchors.fill: parent
            radius: width / 2
            color: buttonHover.hovered ? Qt.rgba(1, 1, 1, 0.22) : Qt.rgba(0, 0, 0, 0.35)
        }

        Widgets.IconLabel {
            anchors.centerIn: parent
            visible: button.icon !== ""
            text: button.icon
            font.pixelSize: button.size * 0.62
            color: "white"
        }

        Image {
            anchors.centerIn: parent
            visible: button.image.toString() !== ""
            source: button.image
            sourceSize: Qt.size(button.size * 0.55, button.size * 0.55)
        }

        Widgets.ToolTipExt {
            visible: buttonHover.hovered && button.tip !== ""
            text: button.tip
            colorContext.palette: VLCStyle.palette
        }

        HoverHandler { id: buttonHover; cursorShape: VLCStyle.pointerCursor }
        TapHandler { onTapped: button.clicked() }
    }

    component Controls: Row {
        property real size: 32
        spacing: size * 0.25

        RoundButton {
            size: parent.size * 0.85
            anchors.verticalCenter: parent.verticalCenter
            icon: VLCIcons.previous
            onClicked: MainPlaylistController.prev()
        }
        RoundButton {
            size: parent.size
            anchors.verticalCenter: parent.verticalCenter
            icon: root.playing ? VLCIcons.pause_filled : VLCIcons.play_filled
            onClicked: MainPlaylistController.togglePlayPause()
        }
        RoundButton {
            size: parent.size * 0.85
            anchors.verticalCenter: parent.verticalCenter
            icon: VLCIcons.next
            onClicked: MainPlaylistController.next()
        }
    }

    // A thin line of the time played; a click moves there
    component Progress: Item {
        height: 10

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 3
            color: Qt.rgba(1, 1, 1, 0.25)

            Rectangle {
                width: parent.width * Math.max(0, Math.min(1, Player.position))
                height: parent.height
                color: root.accent
            }
        }

        TapHandler {
            onTapped: (eventPoint) => { Player.position = eventPoint.position.x / parent.width }
        }
    }

    component Titles: Column {
        property real size: 13
        spacing: 1

        Text {
            width: parent.width
            text: Player.title
            elide: Text.ElideRight
            color: "white"
            font.pixelSize: parent.size
            font.weight: Font.DemiBold
        }
        Text {
            width: parent.width
            text: Player.artist
            visible: text !== ""
            elide: Text.ElideRight
            color: Qt.rgba(1, 1, 1, 0.7)
            font.pixelSize: parent.size * 0.85
        }
    }

    // --- the shapes

    Component {
        id: cardShape

        Item {
            Widgets.ImageExt {
                id: cardCover
                anchors.fill: parent
                radius: 10
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                cache: false
                sourceSize: Qt.size(width * 2, height * 2)
                source: root.coverSource
            }

            // The visualization along the bottom, over a shade
            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: parent.height * 0.4
                radius: 10
                gradient: Gradient {
                    GradientStop { position: 0; color: "transparent" }
                    GradientStop { position: 1; color: Qt.rgba(0, 0, 0, 0.6) }
                }
            }
            AudioVisualizer {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: 6
                height: parent.height * 0.3
                fill: true
                style: root.chosenStyle === 1 ? 6 : root.chosenStyle // no ring in a card: the line
                color: root.accent
                color2: root.accent2
            }

            // On hover: the title, the buttons, the time
            Rectangle {
                anchors.fill: parent
                radius: 10
                color: Qt.rgba(0, 0, 0, 0.5)
                opacity: root.hovered ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: 150 } }

                Titles {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 10
                    size: Math.max(10, Math.min(15, root.width / 18))
                }
                Controls {
                    anchors.centerIn: parent
                    size: Math.max(22, Math.min(48, root.width / 5))
                }
                Progress {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: 8
                }
            }
        }
    }

    Component {
        id: roundShape

        Item {
            readonly property real side: Math.min(width, height)

            AudioVisualizer {
                id: ring
                anchors.centerIn: parent
                width: parent.side
                height: parent.side
                style: 1
                cover: roundCover
                color: root.accent
                color2: root.accent2
            }

            Rectangle {
                anchors.centerIn: roundCover
                width: roundCover.width + 6
                height: width
                radius: width / 2
                color: Qt.rgba(0, 0, 0, 0.55)
                scale: roundCover.scale
            }

            Widgets.ImageExt {
                id: roundCover
                anchors.centerIn: parent
                width: Math.round(parent.side * 0.48)
                height: width
                radius: width / 2
                scale: 1 + ring.level * 0.05
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                cache: false
                sourceSize: Qt.size(width * 2, height * 2)
                source: root.coverSource
            }
        }
    }

    Component {
        id: stripShape

        Item {
            Rectangle {
                anchors.fill: parent
                radius: 10
                color: Qt.rgba(0.08, 0.08, 0.08, 0.88)
                border.color: Qt.rgba(1, 1, 1, 0.08)
            }
            AudioVisualizer {
                anchors.fill: parent
                anchors.margins: 4
                opacity: 0.45
                fill: true
                style: root.chosenStyle === 1 ? 5 : root.chosenStyle // no ring in a strip: the mirror
                color: root.accent
                color2: root.accent2
            }
            Widgets.ImageExt {
                id: stripCover
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 8
                width: parent.height - 16
                height: width
                radius: 6
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                cache: false
                sourceSize: Qt.size(width * 2, height * 2)
                source: root.coverSource
            }
            Controls {
                id: stripControls
                anchors.right: parent.right
                anchors.rightMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                size: Math.max(20, Math.min(36, root.height * 0.45))
            }
            Titles {
                anchors.left: stripCover.right
                anchors.right: stripControls.left
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                size: Math.max(10, Math.min(15, root.height / 5.5))
            }
            Progress {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                anchors.bottomMargin: 2
            }
        }
    }

    Component {
        id: bareShape

        Item {
            // Only seen on hover, to show where to take it
            Rectangle {
                anchors.fill: parent
                radius: 8
                color: Qt.rgba(0, 0, 0, 0.3)
                border.color: Qt.rgba(1, 1, 1, 0.15)
                opacity: root.hovered ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: 150 } }
            }
            AudioVisualizer {
                anchors.fill: parent
                anchors.margins: 2
                fill: true
                style: root.chosenStyle
                color: root.accent
                color2: root.accent2
            }
        }
    }
}
