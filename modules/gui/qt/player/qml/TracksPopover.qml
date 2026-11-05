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
import VLC.Style
import VLC.Util
import VLC.Widgets as Widgets

// Audio and subtitles popover, above its button: a short list of settings
// with their current value; each opens its own page in place.
T.Popup {
    id: root

    padding: VLCStyle.margin_xsmall
    closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutside

    // The synchronization page needs more room than the lists
    readonly property bool _wide: stack.currentItem ? !!stack.currentItem.wide : false
    implicitWidth: _wide ? VLCStyle.dp(560, VLCStyle.scale) : VLCStyle.dp(320, VLCStyle.scale)
    implicitHeight: Math.min(stack.currentItem ? stack.currentItem.implicitHeight : 0,
                             VLCStyle.dp(480, VLCStyle.scale)) + topPadding + bottomPadding

    readonly property ColorContext colorContext: theme

    readonly property var _speeds: [ 0.25, 0.5, 0.75, 1, 1.25, 1.5, 1.75, 2 ]

    function _speedName(rate) {
        return Math.abs(rate - 1) < 0.001 ? qsTr("Normal") : (+rate.toFixed(2)) + "×"
    }

    // Name of the selected tracks of a track model, or "Off"
    function _selected(model, revision) {
        void revision
        const on = model.trackStates().filter(t => t.enabled)
        return on.length === 0 ? qsTr("Off") : on.map(t => t.title).join(", ")
    }

    function _switchOff(model) {
        for (const t of model.trackStates())
            if (t.enabled)
                model.setData(model.index(t.index, 0), false, Qt.CheckStateRole)
    }

    property int _revision: 0
    Connections { target: Player.subtitleTracks; function onDataChanged() { root._revision++ } }
    Connections { target: Player.audioTracks; function onDataChanged() { root._revision++ } }

    onOpened: stack.pop(null)

    background: Rectangle {
        radius: VLCStyle.dp(10, VLCStyle.scale)
        color: Qt.rgba(0.09, 0.09, 0.09, 0.82)
        border.color: Qt.rgba(1, 1, 1, 0.1)
    }

    // One row of a list: a check (or nothing), the label, the value, and
    // an arrow when it opens a page. Self-contained: inline components do
    // not see the ids of this file.
    component Row: T.ItemDelegate {
        id: row

        property string value: ""
        property bool checkMark: false
        property bool opensPage: false

        Layout.fillWidth: true
        implicitHeight: VLCStyle.dp(40, VLCStyle.scale)
        leftPadding: VLCStyle.margin_normal
        rightPadding: VLCStyle.margin_normal

        background: Rectangle {
            radius: VLCStyle.dp(6, VLCStyle.scale)
            color: row.hovered ? Qt.rgba(1, 1, 1, 0.08) : "transparent"
        }

        contentItem: RowLayout {
            spacing: VLCStyle.margin_small

            Text {
                Layout.preferredWidth: VLCStyle.dp(18, VLCStyle.scale)
                visible: !row.opensPage
                text: row.checkMark ? "✓" : ""
                color: "white"
                font.pixelSize: VLCStyle.fontSize_normal
            }

            Text {
                Layout.fillWidth: true
                text: row.text
                color: "white"
                elide: Text.ElideRight
                font.pixelSize: VLCStyle.fontSize_normal
            }

            Text {
                Layout.maximumWidth: VLCStyle.dp(140, VLCStyle.scale)
                visible: row.value !== ""
                text: row.value
                color: Qt.rgba(1, 1, 1, 0.6)
                elide: Text.ElideRight
                font.pixelSize: VLCStyle.fontSize_normal
            }

            Text {
                visible: row.opensPage
                text: "›"
                color: Qt.rgba(1, 1, 1, 0.6)
                font.pixelSize: VLCStyle.fontSize_large
            }
        }
    }

    // Top of a page; clicking it goes back
    component PageHeader: T.ItemDelegate {
        id: header

        Layout.fillWidth: true
        implicitHeight: VLCStyle.dp(40, VLCStyle.scale)
        leftPadding: VLCStyle.margin_small

        background: Rectangle {
            radius: VLCStyle.dp(6, VLCStyle.scale)
            color: header.hovered ? Qt.rgba(1, 1, 1, 0.08) : "transparent"
        }

        contentItem: Text {
            text: "‹  " + header.text
            color: "white"
            font.pixelSize: VLCStyle.fontSize_normal
            font.weight: Font.DemiBold
            verticalAlignment: Text.AlignVCenter
        }
    }

    contentItem: T.StackView {
        // A popup is not an Item, so the palette is found from here down
        readonly property ColorContext colorContext: ColorContext {
            id: theme
            colorSet: ColorContext.Window
            palette: VLCStyle.palette
        }

        id: stack

        clip: true
        initialItem: mainPage
    }

    Component {
        id: mainPage

        ColumnLayout {
            spacing: 0

            Row {
                text: qsTr("Subtitles")
                opensPage: true
                value: root._selected(Player.subtitleTracks, root._revision)
                onClicked: stack.push(subtitlesPage)
            }
            Row {
                visible: Player.audioTracks.count > 0
                text: qsTr("Audio")
                opensPage: true
                value: root._selected(Player.audioTracks, root._revision)
                onClicked: stack.push(audioPage)
            }
            Row {
                text: qsTr("Speed")
                opensPage: true
                value: root._speedName(Player.rate)
                onClicked: stack.push(speedPage)
            }
            Row {
                text: qsTr("Subtitle style")
                opensPage: true
                onClicked: stack.push(stylePage)
            }
            Row {
                text: qsTr("Synchronization")
                opensPage: true
                onClicked: stack.push(syncPage)
            }
            Row {
                text: qsTr("Open subtitle file…")
                onClicked: {
                    root.close()
                    DialogsProvider.loadSubtitlesFile()
                }
            }
        }
    }

    Component {
        id: subtitlesPage

        ColumnLayout {
            spacing: 0

            PageHeader { text: qsTr("Subtitles"); onClicked: stack.pop() }

            Row {
                text: qsTr("Off")
                checkMark: root._selected(Player.subtitleTracks, root._revision) === qsTr("Off")
                onClicked: root._switchOff(Player.subtitleTracks)
            }

            Repeater {
                model: Player.subtitleTracks

                Row {
                    required property var model
                    text: model.display
                    checkMark: model.checked
                    onClicked: model.checked = Player.subtitleTracks.multiSelect ? !model.checked : true
                }
            }
        }
    }

    Component {
        id: audioPage

        ColumnLayout {
            spacing: 0

            PageHeader { text: qsTr("Audio"); onClicked: stack.pop() }

            Repeater {
                model: Player.audioTracks

                Row {
                    required property var model
                    text: model.display
                    checkMark: model.checked
                    onClicked: model.checked = Player.audioTracks.multiSelect ? !model.checked : true
                }
            }
        }
    }

    Component {
        id: speedPage

        ColumnLayout {
            spacing: 0

            PageHeader { text: qsTr("Speed"); onClicked: stack.pop() }

            Repeater {
                model: root._speeds

                Row {
                    required property real modelData
                    text: root._speedName(modelData)
                    checkMark: Math.abs(Player.rate - modelData) < 0.001
                    onClicked: Player.rate = modelData
                }
            }
        }
    }

    Component {
        id: stylePage

        ColumnLayout {
            spacing: 0

            PageHeader { text: qsTr("Subtitle style"); onClicked: stack.pop() }

            SubtitleStyleSection {
                Layout.fillWidth: true
                Layout.margins: VLCStyle.margin_small
                colorContext: root.colorContext
                showTitle: false
            }
        }
    }

    // The synchronization page of the old tracks panel, inside the popover
    QtObject {
        id: syncController
        signal requestBack()
        signal requestAudioPage()
        signal requestSubtitlePage()
        signal requestSubtitleStylePage()
        signal requestPlaybackSpeedPage()
        onRequestBack: stack.pop()
    }

    Component {
        id: syncPage

        TracksPageSubtitle {
            readonly property bool wide: true
            trackMenuController: syncController
        }
    }
}
