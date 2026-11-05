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

import VLC.Player
import VLC.Style
import VLC.Widgets as Widgets

// Audio popover of the volume button: which track plays (or several at once,
// each with its own volume) and a volume boost past 100%.
T.Popup {
    id: root

    // Kept open while the pointer is over the button or the popover
    property bool buttonHovered: false
    readonly property bool _hovered: buttonHovered || popoverHover.hovered || contentHover.hovered

    padding: VLCStyle.margin_normal
    margins: VLCStyle.margin_small

    closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutside

    implicitWidth: VLCStyle.dp(300, VLCStyle.scale)
    implicitHeight: implicitContentHeight + topPadding + bottomPadding

    readonly property ColorContext colorContext: theme

    Timer {
        id: closeTimer
        interval: 400
        onTriggered: root.close()
    }

    on_HoveredChanged: {
        if (_hovered)
            closeTimer.stop()
        else if (opened)
            closeTimer.restart()
    }

    background: Rectangle {
        radius: VLCStyle.dp(10, VLCStyle.scale)
        color: theme.bg.primary
        opacity: 0.96
        border.color: Qt.rgba(1, 1, 1, 0.08)

        HoverHandler {
            id: popoverHover
        }
    }

    contentItem: ColumnLayout {
        // A popup is not an Item, so the palette is found from here down
        readonly property ColorContext colorContext: ColorContext {
            id: theme
            colorSet: ColorContext.Window
            palette: VLCStyle.palette
        }

        // Rows take the hover themselves, so watch it from their parent
        HoverHandler {
            id: contentHover
        }

        spacing: VLCStyle.margin_xsmall

        Text {
            text: qsTr("Audio tracks")
            color: theme.fg.secondary
            font.pixelSize: VLCStyle.fontSize_small
            visible: Player.audioTracks.count > 0
        }

        Repeater {
            model: Player.audioTracks

            ColumnLayout {
                required property var model
                required property int index

                Layout.fillWidth: true
                spacing: 0

                // One track at a time: picking one switches to it.
                // Several at once: each can be switched on and off.
                Widgets.RadioButtonExt {
                    visible: !Player.audioTracks.multiSelect
                    Layout.fillWidth: true
                    text: model.display
                    checked: model.checked
                    onClicked: model.checked = true
                }

                Widgets.CheckBoxExt {
                    visible: Player.audioTracks.multiSelect
                    Layout.fillWidth: true
                    text: model.display
                    checked: model.checked
                    onClicked: model.checked = checked
                }

                // Volume of this track in the mix
                RowLayout {
                    visible: Player.audioTracks.multiSelect && model.checked
                    Layout.fillWidth: true
                    Layout.leftMargin: VLCStyle.margin_large
                    spacing: VLCStyle.margin_small

                    Widgets.SliderExt {
                        Layout.fillWidth: true
                        from: 0
                        to: 2
                        stepSize: 0.05
                        value: model.volume
                        onMoved: model.volume = value
                    }

                    Text {
                        Layout.preferredWidth: VLCStyle.dp(40, VLCStyle.scale)
                        horizontalAlignment: Text.AlignRight
                        text: Math.round(model.volume * 100) + "%"
                        color: theme.fg.secondary
                        font.pixelSize: VLCStyle.fontSize_small
                    }
                }
            }
        }

        Widgets.CheckBoxExt {
            visible: Player.audioTracks.count > 1
            text: qsTr("Play several at once")
            checked: Player.audioTracks.multiSelect
            onClicked: Player.audioTracks.multiSelect = checked
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.topMargin: VLCStyle.margin_xxsmall
            Layout.bottomMargin: VLCStyle.margin_xxsmall
            height: 1
            color: Qt.rgba(1, 1, 1, 0.1)
        }

        // Past 100% only here, on purpose
        RowLayout {
            Layout.fillWidth: true
            spacing: VLCStyle.margin_small

            Text {
                text: qsTr("Boost")
                color: theme.fg.primary
                font.pixelSize: VLCStyle.fontSize_normal
            }

            Widgets.SliderExt {
                Layout.fillWidth: true
                from: 0
                to: 2
                stepSize: 0.01
                value: Player.volume
                color: Player.volume > 1 ? theme.fg.negative : theme.fg.primary
                onMoved: {
                    Player.muted = false
                    Player.volume = value
                }
            }

            Text {
                Layout.preferredWidth: VLCStyle.dp(40, VLCStyle.scale)
                horizontalAlignment: Text.AlignRight
                text: Math.round(Player.volume * 100) + "%"
                color: Player.volume > 1 ? theme.fg.negative : theme.fg.secondary
                font.pixelSize: VLCStyle.fontSize_small
            }
        }
    }
}
