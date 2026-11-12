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
import QtQuick.Layouts
import QtQuick.Templates as T

import VLC.MainInterface
import VLC.Player
import VLC.Playlist
import VLC.Style
import VLC.Util
import VLC.Widgets as Widgets
import VLC.Terrikon

// Controls of the picture in picture window, shown while the pointer is
// over it, as browsers do: title, back to the player and close at the top,
// back/play/forward in the middle, seek bar, time and sound at the bottom.
// The window is dragged by its video.
Item {
    id: root

    readonly property bool shown: hover.hovered || seek.pressed

    function _time(seconds) {
        seconds = Math.max(0, Math.floor(seconds))
        const h = Math.floor(seconds / 3600)
        const m = Math.floor(seconds / 60) % 60
        const s = seconds % 60
        const pad = (n) => (n < 10 ? "0" : "") + n
        return h > 0 ? h + ":" + pad(m) + ":" + pad(s) : m + ":" + pad(s)
    }

    HoverHandler {
        id: hover
    }

    // Dragging the video moves the window; a click plays or pauses
    DragHandler {
        target: null
        onActiveChanged: {
            if (active)
                MainCtx.intfMainWindow.startSystemMove()
        }
    }

    TapHandler {
        onTapped: MainPlaylistController.togglePlayPause()
    }

    // A white icon button on the dark overlay, with a round hover
    component OverlayButton: T.Button {
        id: button

        property string glyph: ""
        property url image: ""
        property real size: VLCStyle.icon_normal
        property string tip: ""

        implicitWidth: size + VLCStyle.margin_small * 2
        implicitHeight: implicitWidth
        hoverEnabled: true

        T.ToolTip.visible: hovered && tip !== ""
        T.ToolTip.delay: 600
        T.ToolTip.text: tip

        background: Rectangle {
            radius: width / 2
            color: button.hovered ? Qt.rgba(1, 1, 1, 0.18) : "transparent"
        }

        contentItem: Item {
            Text {
                visible: button.glyph !== ""
                anchors.centerIn: parent
                text: button.glyph
                color: "white"
                font.family: VLCIcons.fontFamily
                font.pixelSize: button.size
            }
            Image {
                visible: button.image != ""
                anchors.centerIn: parent
                source: button.image
                sourceSize: Qt.size(button.size, button.size)
            }
        }

        HoverHandler { cursorShape: VLCStyle.pointerCursor }
    }

    Item {
        anchors.fill: parent

        opacity: root.shown ? 1 : 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: VLCStyle.duration_short } }

        // Shading so white controls read on any picture
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: parent.height * 0.35
            gradient: Gradient {
                GradientStop { position: 0; color: Qt.rgba(0, 0, 0, 0.65) }
                GradientStop { position: 1; color: "transparent" }
            }
        }
        Rectangle {
            anchors.fill: parent
            color: Qt.rgba(0, 0, 0, 0.2)
        }
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: parent.height * 0.4
            gradient: Gradient {
                GradientStop { position: 0; color: "transparent" }
                GradientStop { position: 1; color: Qt.rgba(0, 0, 0, 0.7) }
            }
        }

        // Top: title, back to the player, close
        RowLayout {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: VLCStyle.margin_xsmall
            spacing: 0

            Text {
                Layout.fillWidth: true
                Layout.leftMargin: VLCStyle.margin_xsmall
                text: Player.name
                color: "white"
                elide: Text.ElideRight
                font.pixelSize: VLCStyle.fontSize_normal
                font.weight: Font.DemiBold
            }

            OverlayButton {
                image: SVGColorImage.colorize("qrc:///icons/compact_player_exit.svg").color1("white").uri()
                size: VLCStyle.icon_small
                tip: qsTr("Back to the player") + TerrikonActions.hint("pip-toggle")
                onClicked: CompactPlayer.leave()
            }

            OverlayButton {
                glyph: VLCIcons.close
                size: VLCStyle.icon_small
                tip: qsTr("Close")
                // As in browsers: the video stops playing and goes back
                onClicked: {
                    MainPlaylistController.pause()
                    CompactPlayer.leave()
                }
            }
        }

        // Middle: 10 s back, play/pause, 10 s forward
        RowLayout {
            anchors.centerIn: parent
            spacing: VLCStyle.margin_normal

            OverlayButton {
                glyph: VLCIcons.skip_back
                tip: qsTr("Back 10 seconds") + TerrikonActions.hint("jump-short")
                onClicked: Player.jumpBwd()
            }

            OverlayButton {
                glyph: (Player.playingState === Player.PLAYING_STATE_PLAYING) ? VLCIcons.pause_filled
                                                                               : VLCIcons.play_filled
                size: VLCStyle.icon_large
                onClicked: MainPlaylistController.togglePlayPause()
            }

            OverlayButton {
                glyph: VLCIcons.skip_for
                tip: qsTr("Forward 10 seconds") + TerrikonActions.hint("jump+short")
                onClicked: Player.jumpFwd()
            }
        }

        // Bottom: seek bar, time, sound
        ColumnLayout {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: VLCStyle.margin_small
            spacing: 0

            Widgets.SliderExt {
                id: seek

                Layout.fillWidth: true
                from: 0
                to: 1
                value: Player.position
                color: "white"
                onMoved: Player.position = value
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 0

                Text {
                    Layout.fillWidth: true
                    Layout.leftMargin: VLCStyle.margin_xxsmall
                    text: root._time(Player.time.toSeconds()) + " / " + root._time(Player.length.toSeconds())
                    color: "white"
                    font.pixelSize: VLCStyle.fontSize_normal
                }

                OverlayButton {
                    glyph: Player.muted ? VLCIcons.volume_muted : VLCIcons.volume_high
                    size: VLCStyle.icon_small
                    tip: (Player.muted ? qsTr("Unmute") : qsTr("Mute")) + TerrikonActions.hint("vol-mute")
                    onClicked: Player.muted = !Player.muted
                }
            }
        }
    }
}
