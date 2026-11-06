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

// Audio popover of the volume button: which track plays, or several at
// once, each with its own volume, and the boost past 100%.
T.Popup {
    id: root

    // Kept open while the pointer is over the button or the popover
    property bool buttonHovered: false
    readonly property bool _hovered: buttonHovered || popoverHover.hovered || contentHover.hovered

    padding: VLCStyle.margin_normal
    margins: VLCStyle.margin_small

    // Clicking the speaker itself (mute) keeps it open
    closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutsideParent

    implicitWidth: VLCStyle.dp(280, VLCStyle.scale)
    implicitHeight: implicitContentHeight + topPadding + bottomPadding

    // Grows and shrinks smoothly when a track row shows up or goes
    height: implicitHeight
    Behavior on height {
        enabled: root.opened
        NumberAnimation { duration: VLCStyle.duration_short; easing.type: Easing.OutCubic }
    }

    readonly property ColorContext colorContext: theme

    // A 0-200% slider with a mark at 100% that the value snaps to from
    // 96 to 107%: going past 100% by mistake is the likelier slip
    component MixSlider: Widgets.SliderExt {
        id: mix

        readonly property real snapBelow: 0.04
        readonly property real snapAbove: 0.07

        from: 0
        to: 2
        stepSize: 0.01

        toolTipTextProvider: (v) => Math.round(snapped(v) * 100) + "%"

        function snapped(v) {
            return (v > 1 - snapBelow && v < 1 + snapAbove) ? 1 : v
        }

        Rectangle {
            x: mix.leftPadding + mix.availableWidth / 2 - width / 2
            y: mix.topPadding + (mix.availableHeight - height) / 2
            width: VLCStyle.dp(2, VLCStyle.scale)
            height: VLCStyle.dp(10, VLCStyle.scale)
            radius: width / 2
            color: Qt.rgba(1, 1, 1, 0.45)
        }
    }

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
        color: Qt.rgba(0.09, 0.09, 0.09, MainCtx.overlayOpacity)
        border.color: Qt.rgba(1, 1, 1, 0.1)

        HoverHandler {
            id: popoverHover
        }
    }

    contentItem: Item {
        // A popup is not an Item, so the palette is found from here down
        readonly property ColorContext colorContext: ColorContext {
            id: theme
            colorSet: ColorContext.Window
            palette: VLCStyle.palette
        }

        implicitWidth: column.implicitWidth
        implicitHeight: column.implicitHeight
        clip: true

        // Rows take the hover themselves, so watch it from their parent
        HoverHandler {
            id: contentHover
        }

        ColumnLayout {
            id: column

            width: parent.width
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

                        MixSlider {
                            Layout.fillWidth: true
                            value: model.volume
                            onMoved: model.volume = snapped(value)
                        }

                        Text {
                            Layout.preferredWidth: VLCStyle.dp(34, VLCStyle.scale)
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
                text: qsTr("Several tracks")
                checked: Player.audioTracks.multiSelect
                onClicked: Player.audioTracks.multiSelect = checked
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.topMargin: VLCStyle.margin_xxsmall
                Layout.bottomMargin: VLCStyle.margin_xxsmall
                implicitHeight: 1
                color: Qt.rgba(1, 1, 1, 0.1)
                visible: Player.audioTracks.count > 0
            }

            // Past 100% only here, on purpose: the bar's slider and the
            // wheel stop at 100%
            Text {
                text: qsTr("Boost")
                color: theme.fg.secondary
                font.pixelSize: VLCStyle.fontSize_small
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: VLCStyle.margin_small

                MixSlider {
                    Layout.fillWidth: true
                    value: Player.volume
                    color: Player.volume > 1 ? theme.fg.negative : theme.fg.primary
                    onMoved: {
                        Player.muted = false
                        Player.volume = snapped(value)
                    }
                }

                Text {
                    Layout.preferredWidth: VLCStyle.dp(34, VLCStyle.scale)
                    horizontalAlignment: Text.AlignRight
                    text: Math.round(Player.volume * 100) + "%"
                    color: Player.volume > 1 ? theme.fg.negative : theme.fg.secondary
                    font.pixelSize: VLCStyle.fontSize_small
                }
            }
        }
    }
}
