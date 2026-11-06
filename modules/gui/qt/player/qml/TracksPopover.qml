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
import VLC.Terrikon
import VLC.Style
import VLC.Util
import VLC.Widgets as Widgets

// Audio and subtitles popover, above its button: a short list of settings
// with their current value; each opens its own page in place.
T.Popup {
    id: root

    padding: VLCStyle.margin_xsmall
    // Stays inside the window, whichever side of the bar its button is on
    margins: VLCStyle.margin_small
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
        color: Qt.rgba(0.09, 0.09, 0.09, MainCtx.overlayOpacity)
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
                text: qsTr("Sound waves")
                opensPage: true
                value: root._waveStyles.find(s => s.id === MainCtx.waveformStyle)?.name ?? ""
                onClicked: stack.push(wavesPage)
            }
            Row {
                text: qsTr("Transparency")
                opensPage: true
                onClicked: stack.push(opacityPage)
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

    // Looks of the sound waves along the seek bar, each shown on the sound
    // of what is playing
    readonly property var _waveStyles: [
        { id: "curve", name: qsTr("Smooth line"), style: WaveformView.Curve },
        { id: "bars", name: qsTr("Thin columns"), style: WaveformView.Bars },
        { id: "mirror", name: qsTr("Mirrored"), style: WaveformView.Mirror },
        { id: "split", name: qsTr("Above and below the bar (two tracks)"), style: WaveformView.Curve }
    ]

    Component {
        id: wavesPage

        ColumnLayout {
            spacing: 0

            PageHeader { text: qsTr("Sound waves"); onClicked: stack.pop() }

            // Cached on disk by the seek bar's loader, so this is quick
            WaveformLoader {
                id: previewLoader
                Component.onCompleted: loadUrl(Player.url)
            }

            Repeater {
                model: root._waveStyles

                T.ItemDelegate {
                    id: choice

                    required property var modelData
                    readonly property bool current: MainCtx.waveformStyle === modelData.id

                    Layout.fillWidth: true
                    padding: VLCStyle.margin_small
                    leftPadding: VLCStyle.margin_normal
                    rightPadding: VLCStyle.margin_normal
                    // A template control has no size of its own
                    implicitWidth: implicitContentWidth + leftPadding + rightPadding
                    implicitHeight: implicitContentHeight + topPadding + bottomPadding

                    onClicked: MainCtx.waveformStyle = modelData.id

                    background: Rectangle {
                        radius: VLCStyle.dp(6, VLCStyle.scale)
                        color: choice.current ? Qt.rgba(1, 1, 1, 0.1)
                             : choice.hovered ? Qt.rgba(1, 1, 1, 0.06) : "transparent"
                    }

                    contentItem: ColumnLayout {
                        spacing: VLCStyle.margin_xxsmall

                        Text {
                            text: (choice.current ? "✓  " : "") + choice.modelData.name
                            color: "white"
                            font.pixelSize: VLCStyle.fontSize_normal
                        }

                        // The bar with the waves over it (and under it, split)
                        Item {
                            Layout.fillWidth: true
                            implicitHeight: VLCStyle.dp(choice.modelData.id === "split" ? 40 : 30, VLCStyle.scale)

                            readonly property var _tracks: Player.audioTracks.trackStates()
                                                               .filter(t => t.enabled).map(t => t.index)

                            WaveformView {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                height: VLCStyle.dp(24, VLCStyle.scale)
                                loader: previewLoader
                                colors: [ "#ffffff", "#d0d0d0" ]
                                style: choice.modelData.style
                                tracks: choice.modelData.id === "split" ? parent._tracks.slice(0, 1) : parent._tracks
                            }

                            Rectangle {
                                y: VLCStyle.dp(26, VLCStyle.scale)
                                width: parent.width
                                height: VLCStyle.dp(2, VLCStyle.scale)
                                color: Qt.rgba(1, 1, 1, 0.4)
                            }

                            WaveformView {
                                visible: choice.modelData.id === "split"
                                anchors.left: parent.left
                                anchors.right: parent.right
                                y: VLCStyle.dp(29, VLCStyle.scale)
                                height: VLCStyle.dp(11, VLCStyle.scale)
                                loader: previewLoader
                                colors: [ "#d0d0d0" ]
                                style: WaveformView.Curve
                                tracks: parent._tracks.slice(1, 2)
                                transform: Scale { origin.y: VLCStyle.dp(5.5, VLCStyle.scale); yScale: -1 }
                            }
                        }
                    }
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

    Component {
        id: opacityPage

        ColumnLayout {
            spacing: VLCStyle.margin_xsmall

            PageHeader { text: qsTr("Transparency"); onClicked: stack.pop() }

            component OpacitySlider: ColumnLayout {
                id: opacityRow

                property alias text: opacityLabel.text
                property real value
                signal moved(real value)

                Layout.fillWidth: true
                Layout.leftMargin: VLCStyle.margin_normal
                Layout.rightMargin: VLCStyle.margin_normal
                Layout.bottomMargin: VLCStyle.margin_small
                spacing: VLCStyle.margin_xxsmall

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        id: opacityLabel
                        Layout.fillWidth: true
                        color: theme.fg.secondary
                        font.pixelSize: VLCStyle.fontSize_small
                    }
                    Text {
                        text: Math.round(opacityRow.value * 100) + "%"
                        color: theme.fg.secondary
                        font.pixelSize: VLCStyle.fontSize_small
                    }
                }

                Widgets.SliderExt {
                    Layout.fillWidth: true
                    Layout.preferredWidth: VLCStyle.dp(240, VLCStyle.scale)
                    from: 0.2
                    to: 1
                    stepSize: 0.01
                    value: opacityRow.value
                    toolTipTextProvider: (v) => Math.round(v * 100) + "%"
                    onMoved: opacityRow.moved(value)
                    onPressedChanged: if (!pressed) MainCtx.saveConfig()
                }
            }

            OpacitySlider {
                text: qsTr("See-through parts: these menus, messages over the video")
                value: MainCtx.overlayOpacity
                onMoved: (v) => MainCtx.overlayOpacity = v
            }

            OpacitySlider {
                text: qsTr("Solid parts: the queue, the clip panel")
                value: MainCtx.panelOpacity
                onMoved: (v) => MainCtx.panelOpacity = v
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
