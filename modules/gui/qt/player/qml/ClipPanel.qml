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
import VLC.Util
import VLC.Widgets as Widgets
import VLC.Terrikon

// Clip mode card, above the seek bar on the left. The range is edited on
// the seek bar and plays in a loop; this card sets what the clip contains
// and saves or copies it.
FocusScope {
    id: root

    // Original: the video is copied as it is. Re-encoded: with a codec, at
    // its best quality or to fit a file size.
    property bool reencode: false
    property string codec: "h264"
    property bool fitSize: false
    property int targetMB: 25
    readonly property string _preset: !reencode ? "copy" : (fitSize ? "size" : "quality")

    // The clip's audio: starts as the player plays it, then edited here
    // without touching playback. [{ index, title, enabled, volume }]
    property var tracks: []
    property bool mix: true
    readonly property int _includedTracks: tracks.filter(t => t.enabled).length

    readonly property bool _running: exporter.running
    property string _resultPath: ""
    property bool _resultCopied: false
    property string _error: ""

    implicitWidth: Math.min(VLCStyle.dp(560, VLCStyle.scale), parent ? parent.width : 0)
    implicitHeight: column.implicitHeight + 2 * VLCStyle.margin_normal

    visible: ClipSelection.active

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.Window
    }

    function _currentSeconds() {
        return Player.position * ClipSelection.mediaLength
    }

    function _setTrack(i, key, value) {
        const copy = tracks.slice()
        copy[i] = Object.assign({}, copy[i])
        copy[i][key] = value
        tracks = copy
    }

    function _params(extra) {
        const p = {
            input: Player.url.toString(),
            start: ClipSelection.start,
            end: ClipSelection.end,
            mediaLength: ClipSelection.mediaLength,
            tracks: tracks.map(t => ({ index: t.index, enabled: t.enabled, volume: t.volume })),
            merge: mix && _includedTracks > 1,
            preset: _preset,
            codec: codec,
            vertical: ClipSelection.vertical,
            cropX: ClipSelection.cropX,
            cropY: ClipSelection.cropY,
            cropZoom: ClipSelection.cropZoom
        }
        if (_preset === "size")
            p.targetBytes = targetMB * 1024 * 1024
        for (const key in extra)
            p[key] = extra[key]
        return p
    }

    function _formatSize(bytes) {
        if (bytes <= 0)
            return ""
        const mb = bytes / (1024 * 1024)
        return mb < 10 ? qsTr("%1 MB").arg(mb.toFixed(1)) : qsTr("%1 MB").arg(Math.round(mb))
    }

    function _start(params) {
        _resultPath = ""
        _error = ""
        _resultCopied = !!params.copyToClipboard && !!params.temporary
        if (!exporter.start(params))
            _error = qsTr("Could not start making the clip.")
    }

    function save() {
        _start(_params({}))
    }

    function saveAs() {
        const path = exporter.askSavePath(exporter.defaultOutput(_params({})))
        if (path !== "")
            _start(_params({ output: path }))
    }

    function copy() {
        _start(_params({ temporary: true, copyToClipboard: true }))
    }

    ClipExporter {
        id: exporter

        onFinished: (ok, output, error) => {
            root._resultPath = ok ? output : ""
            root._error = ok ? "" : error
        }
    }

    Connections {
        target: ClipSelection

        function onActiveChanged() {
            if (!ClipSelection.active)
                return
            root.tracks = Player.audioTracks.trackStates()
            root._resultPath = ""
            root._error = ""
            // Show the range from its start; it then plays in a loop
            if (ClipSelection.mediaLength > 0)
                Player.position = ClipSelection.start / ClipSelection.mediaLength
        }

        function onRangeChanged() {
            root._resultPath = ""
            root._error = ""
        }
    }

    // Loop the range: past its end, back to its start. A seek far beyond the
    // end (on purpose, on the seek bar) is left alone.
    Connections {
        target: Player
        enabled: ClipSelection.active && !root._running

        function onPositionChanged() {
            const now = root._currentSeconds()
            if (now >= ClipSelection.end && now < ClipSelection.end + 1.5)
                Player.position = ClipSelection.start / ClipSelection.mediaLength
        }
    }

    Connections {
        target: TerrikonActions
        function onClipExportRequested() {
            if (!root._running)
                root.save()
        }
    }

    // A thin underline marks the chosen one, as in the subtitle style page
    component Choice: Widgets.TextToolButton {
        id: choice

        property bool current: false

        font.bold: current

        Rectangle {
            visible: choice.current
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: choice.leftPadding
            anchors.rightMargin: choice.rightPadding
            height: VLCStyle.dp(2, VLCStyle.scale)
            radius: height / 2
            color: "white"
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: VLCStyle.dp(10, VLCStyle.scale)
        color: theme.bg.primary
        opacity: 0.94
    }

    ColumnLayout {
        id: column

        anchors.fill: parent
        anchors.margins: VLCStyle.margin_normal

        spacing: VLCStyle.margin_small

        // Title, range, close
        RowLayout {
            Layout.fillWidth: true
            spacing: VLCStyle.margin_small

            Text {
                text: qsTr("Clip")
                color: theme.fg.primary
                font.pixelSize: VLCStyle.fontSize_large
                font.weight: Font.DemiBold
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("%1 – %2 · %3 s")
                        .arg(ClipSelection.formatTime(ClipSelection.start))
                        .arg(ClipSelection.formatTime(ClipSelection.end))
                        .arg(ClipSelection.duration.toFixed(1))
                color: theme.fg.secondary
                font.pixelSize: VLCStyle.fontSize_normal
            }

            Widgets.IconToolButton {
                text: VLCIcons.close
                description: qsTr("Close clip mode") + TerrikonActions.hint("clip-toggle")
                enabled: !root._running
                onClicked: ClipSelection.close()
            }
        }

        // Audio: which tracks go in, how loud, mixed or not
        Text {
            visible: root.tracks.length > 0
            text: qsTr("Audio")
            color: theme.fg.secondary
            font.pixelSize: VLCStyle.fontSize_small
        }

        Repeater {
            model: root.tracks

            RowLayout {
                required property var modelData
                required property int index

                Layout.fillWidth: true
                spacing: VLCStyle.margin_small

                Widgets.CheckBoxExt {
                    Layout.preferredWidth: VLCStyle.dp(170, VLCStyle.scale)
                    text: modelData.title
                    checked: modelData.enabled
                    enabled: !root._running
                    onClicked: root._setTrack(index, "enabled", checked)
                }

                Widgets.SliderExt {
                    Layout.fillWidth: true
                    from: 0
                    to: 2
                    stepSize: 0.05
                    value: modelData.volume
                    enabled: modelData.enabled && !root._running
                    onMoved: root._setTrack(index, "volume", value)
                }

                Text {
                    Layout.preferredWidth: VLCStyle.dp(44, VLCStyle.scale)
                    horizontalAlignment: Text.AlignRight
                    text: Math.round(modelData.volume * 100) + "%"
                    color: theme.fg.secondary
                    font.pixelSize: VLCStyle.fontSize_small
                }
            }
        }

        Widgets.CheckBoxExt {
            visible: root._includedTracks > 1
            text: qsTr("Mix into one track")
            checked: root.mix
            enabled: !root._running
            onClicked: root.mix = checked
        }

        // Format and shape
        RowLayout {
            Layout.fillWidth: true
            spacing: VLCStyle.margin_xxsmall

            Choice {
                text: qsTr("Original")
                current: !root.reencode
                enabled: !root._running
                onClicked: root.reencode = false
            }
            Choice {
                text: qsTr("Re-encode")
                current: root.reencode
                enabled: !root._running
                onClicked: root.reencode = true
            }

            Item { Layout.fillWidth: true }

            Widgets.CheckBoxExt {
                text: qsTr("Vertical 9:16")
                checked: ClipSelection.vertical
                enabled: !root._running
                onClicked: ClipSelection.vertical = checked
            }
        }

        // Re-encoding: codec, then quality or a file size to fit
        RowLayout {
            Layout.fillWidth: true
            visible: root.reencode
            spacing: VLCStyle.margin_xxsmall

            Repeater {
                model: [ { key: "h264", name: "H.264" }, { key: "hevc", name: "H.265" }, { key: "av1", name: "AV1" } ]

                Choice {
                    required property var modelData
                    text: modelData.name
                    current: root.codec === modelData.key
                    enabled: !root._running
                    onClicked: root.codec = modelData.key
                }
            }

            Item { Layout.fillWidth: true }

            Choice {
                text: qsTr("Best quality")
                current: !root.fitSize
                enabled: !root._running
                onClicked: root.fitSize = false
            }
            Choice {
                text: qsTr("Fit in")
                current: root.fitSize
                enabled: !root._running
                onClicked: root.fitSize = true
            }
            Widgets.SpinBoxExt {
                visible: root.fitSize
                from: 1
                to: 4000
                value: root.targetMB
                editable: true
                enabled: !root._running
                onValueModified: root.targetMB = value
            }
            Text {
                visible: root.fitSize
                text: qsTr("MB")
                color: theme.fg.secondary
                font.pixelSize: VLCStyle.fontSize_normal
            }
        }

        // What to do with it, then its size under the buttons
        RowLayout {
            Layout.fillWidth: true
            spacing: VLCStyle.margin_small

            Item { Layout.fillWidth: true }

            Widgets.ButtonExt {
                visible: !root._running
                text: qsTr("Copy")
                onClicked: root.copy()
            }
            Widgets.ButtonExt {
                visible: !root._running
                text: qsTr("Save as…")
                onClicked: root.saveAs()
            }
            Widgets.ActionButtonPrimary {
                visible: !root._running
                text: qsTr("Save") + TerrikonActions.hint("clip-export")
                onClicked: root.save()
            }

            Widgets.ProgressBarExt {
                visible: root._running
                Layout.preferredWidth: VLCStyle.dp(160, VLCStyle.scale)
                from: 0
                to: 1
                value: exporter.progress
            }
            Widgets.ButtonExt {
                visible: root._running
                text: qsTr("Cancel")
                onClicked: exporter.cancel()
            }
        }

        // The same for all three: copy, save as, save
        Text {
            Layout.alignment: Qt.AlignRight
            visible: !root._running && text !== ""
            text: {
                const size = root._formatSize(exporter.estimateBytes(root._params({})))
                return size === "" ? "" : (root._preset === "size" ? "≤ " : "≈ ") + size
            }
            color: theme.fg.secondary
            font.pixelSize: VLCStyle.fontSize_normal
        }

        // What happened
        RowLayout {
            Layout.fillWidth: true
            visible: !root._running && (root._resultPath !== "" || root._error !== "")
            spacing: VLCStyle.margin_small

            Text {
                Layout.fillWidth: true
                elide: Text.ElideMiddle
                text: {
                    if (root._error !== "")
                        return root._error
                    if (root._resultCopied)
                        return qsTr("Copied. Paste it into a chat or a folder.")
                    return qsTr("Saved to %1").arg(root._resultPath)
                }
                color: root._error !== "" ? theme.fg.negative : theme.fg.primary
                font.pixelSize: VLCStyle.fontSize_normal
            }

            Widgets.TextToolButton {
                visible: root._resultPath !== "" && !root._resultCopied
                text: qsTr("Show in folder")
                onClicked: exporter.reveal(root._resultPath)
            }
        }
    }
}
